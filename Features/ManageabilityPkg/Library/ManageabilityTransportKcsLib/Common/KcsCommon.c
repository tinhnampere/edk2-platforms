/** @file

  KCS instance of Manageability Transport Library

  Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/
#include <Uefi.h>
#include <IndustryStandard/IpmiKcs.h>
#include <Library/BaseMemoryLib.h>
#include <Library/IoLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/TimerLib.h>

#include "ManageabilityTransportKcs.h"

extern MANAGEABILITY_TRANSPORT_KCS_HARDWARE_INFO  mKcsHardwareInfo;

/**
  This function waits for parameter Flag to set.
  Checks status flag in every 1ms internal till 5 seconds elapses.

  @param[in]  Flag        KCS Flag to test.
  @retval     EFI_SUCCESS The KCS flag under test is set.
  @retval     EFI_TIMEOUT The KCS flag didn't set in 5 second windows.
**/
EFI_STATUS
WaitStatusSet (
  IN  UINT8  Flag
  )
{
  UINT64  Timeout = 0;

  while (!(KcsRegisterRead8 (KCS_REG_STATUS) & Flag)) {
    MicroSecondDelay (IPMI_KCS_TIMEOUT_1MS);
    Timeout = Timeout + IPMI_KCS_TIMEOUT_1MS;
    if (Timeout >= IPMI_KCS_TIMEOUT_5_SEC) {
      return EFI_TIMEOUT;
    }
  }

  return EFI_SUCCESS;
}

/**
  This function waits for parameter Flag to get cleared.
  Checks status flag in every 1ms internal till 5 seconds elapses.

  @param[in]  Flag        KCS Flag to test.

  @retval     EFI_SUCCESS The KCS flag under test is clear.
  @retval     EFI_TIMEOUT The KCS flag didn't cleared in 5 second windows.
**/
EFI_STATUS
WaitStatusClear (
  IN  UINT8  Flag
  )
{
  UINT64  Timeout = 0;

  while (KcsRegisterRead8 (KCS_REG_STATUS) & Flag) {
    MicroSecondDelay (IPMI_KCS_TIMEOUT_1MS);
    Timeout = Timeout + IPMI_KCS_TIMEOUT_1MS;
    if (Timeout >= IPMI_KCS_TIMEOUT_5_SEC) {
      return EFI_TIMEOUT;
    }
  }

  return EFI_SUCCESS;
}

/**
  This function validates KCS OBF bit.
  Checks whether OBF bit is set or not.

  @retval EFI_SUCCESS    OBF bit is set.
  @retval EFI_NOT_READY  OBF bit is not set.
**/
EFI_STATUS
ClearOBF (
  VOID
  )
{
  if (KcsRegisterRead8 (KCS_REG_STATUS) & IPMI_KCS_OBF) {
    KcsRegisterRead8 (KCS_REG_DATA_IN); // read the data to clear the OBF
    if (KcsRegisterRead8 (KCS_REG_STATUS) & IPMI_KCS_OBF) {
      return EFI_NOT_READY;
    }
  }

  return EFI_SUCCESS;
}

/**
  This function writes/sends data to the KCS port.
  Algorithm is based on flow chart provided in IPMI spec 2.0
  Figure 9-6, KCS Interface BMC to SMS Write Transfer Flow Chart

  @param[in]  NetFunction           Net function of the command.
  @param[in]  Command               IPMI Command.
  @param[in]  RequestData           Command Request Data, could be NULL.
                                    RequestDataSize must be zero, if RequestData
                                    is NULL.
  @param[in]  RequestDataSize       Size of Command Request Data.

  @retval     EFI_SUCCESS           The command byte stream was successfully
                                    submit to the device and a response was
                                    successfully received.
  @retval     EFI_NOT_FOUND         The command was not successfully sent to the
                                    device or a response was not successfully
                                    received from the device.
  @retval     EFI_NOT_READY         Ipmi Device is not ready for Ipmi command
                                    access.
  @retval     EFI_DEVICE_ERROR      Ipmi Device hardware error.
  @retval     EFI_TIMEOUT           The command time out.
  @retval     EFI_UNSUPPORTED       The command was not successfully sent to
                                    the device.
  @retval     EFI_OUT_OF_RESOURCES  The resource allocation is out of resource or
                                    data size error.
**/
EFI_STATUS
KcsTransportWrite (
  IN  UINT8   NetFunction,
  IN  UINT8   Command,
  IN  UINT8   *RequestData OPTIONAL,
  IN  UINT32  RequestDataSize
  )
{
  EFI_STATUS  Status;
  UINT32      Length;
  UINT8       *Buffer;
  UINT8       *BufferPtr;

  // Validation on RequestData and RequestDataSize.
  if ((RequestData == NULL && RequestDataSize != 0) ||
      (RequestData != NULL && RequestDataSize == 0)
      ) {
    DEBUG ((DEBUG_ERROR, "%a: Mismatched values of RequestData or RequestDataSize.\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  Length = sizeof (NetFunction) + sizeof (Command);
  if (RequestData != NULL) {
    Length = Length + RequestDataSize;
  }

  Buffer = AllocateZeroPool (Length);
  if (Buffer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  //
  // Buffer[0] = NetFunction
  // Buffer[1] = Command
  // Buffer [2..RequestDataSize] = RequestData
  //
  BufferPtr = Buffer;
  CopyMem (BufferPtr, &NetFunction, sizeof (NetFunction));
  BufferPtr += sizeof (NetFunction);
  CopyMem (BufferPtr, &Command, sizeof (Command));
  BufferPtr += sizeof (Command);
  if (Length > (sizeof (NetFunction) + sizeof (Command))) {
    CopyMem (BufferPtr, RequestData, RequestDataSize);
  }

  BufferPtr = Buffer;

  // Step 1. wait for IBF to get clear
  Status = WaitStatusClear (IPMI_KCS_IBF);
  if (EFI_ERROR (Status)) {
    FreePool (Buffer);
    return Status;
  }

  // Step 2. clear OBF
  if (EFI_ERROR (ClearOBF ())) {
    FreePool (Buffer);
    return EFI_NOT_READY;
  }

  // Step 3. WR_START to CMD, phase=wr_start
  KcsRegisterWrite8 (KCS_REG_COMMAND, IPMI_KCS_CONTROL_CODE_WRITE_START);

  // Step 4. wait for IBF to get clear
  Status = WaitStatusClear (IPMI_KCS_IBF);
  if (EFI_ERROR (Status)) {
    FreePool (Buffer);
    return Status;
  }

  // Step 5. check state it should be WRITE_STATE, else exit with error
  if (IPMI_KCS_GET_STATE (KcsRegisterRead8 (KCS_REG_STATUS)) != IPMI_KCS_WRITE_STATE) {
    FreePool (Buffer);
    return EFI_NOT_READY;
  }

  // Step 6, Clear OBF
  if (EFI_ERROR (ClearOBF ())) {
    FreePool (Buffer);
    return EFI_NOT_READY;
  }

  while (Length > 1) {
    // Step 7, phase wr_data, write one byte of Data
    KcsRegisterWrite8 (KCS_REG_DATA_OUT, *BufferPtr);
    Length--;
    BufferPtr++;

    // Step 8. wait for IBF clear
    Status = WaitStatusClear (IPMI_KCS_IBF);
    if (EFI_ERROR (Status)) {
      FreePool (Buffer);
      return Status;
    }

    // Step 9. check state it should be WRITE_STATE, else exit with error
    if (IPMI_KCS_GET_STATE (KcsRegisterRead8 (KCS_REG_STATUS)) != IPMI_KCS_WRITE_STATE) {
      FreePool (Buffer);
      return EFI_NOT_READY;
    }

    // Step 10
    if (EFI_ERROR (ClearOBF ())) {
      FreePool (Buffer);
      return EFI_NOT_READY;
    }

    //
    // Step 11, check for DATA completion if more than one byte;
    // if still need to be transferred then go to step 7 and repeat
    //
  }

  // Step 12, WR_END  to CMD
  KcsRegisterWrite8 (KCS_REG_COMMAND, IPMI_KCS_CONTROL_CODE_WRITE_END);

  // Step 13. wait for IBF to get clear
  Status = WaitStatusClear (IPMI_KCS_IBF);
  if (EFI_ERROR (Status)) {
    FreePool (Buffer);
    return Status;
  }

  // Step 14. check state it should be WRITE_STATE, else exit with error
  if (IPMI_KCS_GET_STATE (KcsRegisterRead8 (KCS_REG_STATUS)) != IPMI_KCS_WRITE_STATE) {
    FreePool (Buffer);
    return EFI_NOT_READY;
  }

  // Step 15
  if (EFI_ERROR (ClearOBF ())) {
    FreePool (Buffer);
    return EFI_NOT_READY;
  }

  // Step 16, write the last byte
  KcsRegisterWrite8 (KCS_REG_DATA_OUT, *BufferPtr);
  FreePool (Buffer);
  return EFI_SUCCESS;
}

/**
  This function sends/receives data from KCS port.
  Algorithm is based on flow chart provided in IPMI spec 2.0
  Figure 9-7, KCS Interface BMC to SMS Read Transfer Flow Chart

  @param [in]       DataBytes             Buffer to hold the read Data.
  @param [in, out]  Length                Number of Bytes read from KCS port.
  @retval           EFI_SUCCESS           The command byte stream was
                                          successfully submit to the device and
                                          a response was successfully received.
  @retval           EFI_NOT_FOUND         The command was not successfully sent
                                          to the device or a response was not
                                          successfully received from the
                                          device.
  @retval           EFI_NOT_READY         Ipmi Device is not ready for Ipmi
                                          command access.
  @retval           EFI_DEVICE_ERROR      Ipmi Device hardware error.
  @retval           EFI_TIMEOUT           The command time out.
  @retval           EFI_UNSUPPORTED       The command was not successfully set
                                          to the device.
  @retval           EFI_OUT_OF_RESOURCES  The resource allocation is out of
                                          resource or data size error.
**/
EFI_STATUS
KcsTransportRead (
  OUT    UINT8   *DataByte,
  IN OUT UINT32  *Length
  )
{
  EFI_STATUS  Status;
  UINT32      ReadLength;

  if (DataByte == NULL || *Length == 0) {
    DEBUG ((DEBUG_ERROR, "%a: Either DataByte is NULL or Length is 0.\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }
  ReadLength = 0;
  while (ReadLength < *Length) {
    // Step 1. wait for IBF to get clear
    Status = WaitStatusClear (IPMI_KCS_IBF);
    if (EFI_ERROR (Status)) {
      *Length = ReadLength;
      return Status;
    }

    // Step 2. check state it should be READ_STATE, else exit with error
    if (IPMI_KCS_GET_STATE (KcsRegisterRead8 (KCS_REG_STATUS)) == IPMI_KCS_READ_STATE) {
      // Step 2.1.1 check of OBF to get clear
      Status = WaitStatusSet (IPMI_KCS_OBF);
      if (EFI_ERROR (Status)) {
        *Length = ReadLength;
        return Status;
      }

      // Step 2.1.2 read data from data out
      DataByte[ReadLength++] = KcsRegisterRead8 (KCS_REG_DATA_IN);
      Status                 = WaitStatusClear (IPMI_KCS_IBF);
      if (EFI_ERROR (Status)) {
        *Length = ReadLength;
        return Status;
      }

      // Step 2.1.3 Write READ byte to data in register.
      KcsRegisterWrite8 (KCS_REG_DATA_OUT, IPMI_KCS_CONTROL_CODE_READ);
    } else if (IPMI_KCS_GET_STATE (KcsRegisterRead8 (KCS_REG_STATUS)) == IPMI_KCS_IDLE_STATE) {
      // Step 2.2.1
      Status = WaitStatusSet (IPMI_KCS_OBF);
      if (EFI_ERROR (Status)) {
        *Length = ReadLength;
        return Status;
      }

      // Step 2.2.2 read dummy data
      KcsRegisterRead8 (KCS_REG_DATA_IN); // Dummy read as per IPMI spec
      *Length = ReadLength;
      return EFI_SUCCESS;
    } else {
      *Length = ReadLength;
      return EFI_DEVICE_ERROR;
    }
  }

  *Length = ReadLength;
  return EFI_SUCCESS;
}

/**
  This service communicates with BMC using KCS protocol.

  @param[in]      NetFunction           Net function of the command.
  @param[in]      Command               IPMI Command.
  @param[in]      RequestData           Command Request Data.
  @param[in]      RequestDataSize       Size of Command Request Data.
  @param[out]     ResponseData          Command Response Data. The completion
                                        code is the first byte of response
                                        data.
  @param[in, out] ResponseDataSize      Size of Command Response Data.
                                        When IN, it is the expected data size
                                        of response data.
                                        When OUT, it is the data size of response
                                        exactly returned.
  @retval         EFI_SUCCESS           The command byte stream was
                                        successfully submit to the device and a
                                        response was successfully received.
  @retval         EFI_NOT_FOUND         The command was not successfully sent
                                        to the device or a response was not
                                        successfully received from the device.
  @retval         EFI_NOT_READY         Ipmi Device is not ready for Ipmi
                                        command access.
  @retval         EFI_DEVICE_ERROR      Ipmi Device hardware error.
  @retval         EFI_TIMEOUT           The command time out.
  @retval         EFI_UNSUPPORTED       The command was not successfully sent to
                                        the device.
  @retval         EFI_OUT_OF_RESOURCES  The resource allocation is out of
                                        resource or data size error.
**/
EFI_STATUS
EFIAPI
KcsTransportSendCommand (
  IN  UINT8      NetFunction,
  IN  UINT8      Command,
  IN  UINT8      *RequestData OPTIONAL,
  IN  UINT32     RequestDataSize,
  OUT UINT8      *ResponseData OPTIONAL,
  IN OUT UINT32  *ResponseDataSize OPTIONAL
  )
{
  EFI_STATUS                Status;
  UINT32                    RspHeaderSize;
  IPMI_KCS_RESPONSE_HEADER  RspHeader;

  if ((RequestData != NULL) && (RequestDataSize == 0)) {
    DEBUG((DEBUG_ERROR, "%a: Mismatched values of RequestData and RequestDataSize\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  if ((ResponseData != NULL) && ((ResponseDataSize != NULL) && (*ResponseDataSize == 0))) {
    DEBUG((DEBUG_ERROR, "%a: Mismatched values of ResponseData and ResponseDataSize\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  Status = KcsTransportWrite (
             (NetFunction << 2),
             Command,
             RequestData,
             RequestDataSize
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "IPMI KCS Write Failed with Status(%r) for NetFunction(0x%x)," \
      " Command(0x%x).\n",
      Status,
      NetFunction,
      Command
      ));
    return Status;
  }

  //
  // Read the response header
  RspHeaderSize = sizeof (IPMI_KCS_RESPONSE_HEADER);
  Status        = KcsTransportRead ((UINT8 *)&RspHeader, &RspHeaderSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "IPMI KCS read response header failed Status(%r), " \
      "RspNetFunctionLun = 0x%x, " \
      "Comamnd = 0x%x \n",
      Status,
      RspHeader.NetFunc,
      RspHeader.Command
      ));
    return (Status);
  }

  Status = KcsTransportRead ((UINT8 *)ResponseData, ResponseDataSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "IPMI KCS response read Failed with Status(%r) for NetFunction(0x%x)," \
      " Command(0x%x).\n",
      Status,
      NetFunction,
      Command
      ));
  }

  return Status;
}

/**
  This function reads 8-bit value from register address.

  @param[in]      Address               This represents either 16-bit IO address
                                        or 32-bit memory mapped address.

  @retval         UINT8                 8-bit value.
**/
UINT8
KcsRegisterRead8 (
  MANAGEABILITY_TRANSPORT_HARDWARE_IO  Address
  )
{
  UINT8  Value;

  if (mKcsHardwareInfo.MemoryMap == MANAGEABILITY_TRANSPORT_KCS_MEMORY_MAP_IO) {
    // Read 8-bit value from 32-bit Memory mapped address.
    Value = MmioRead8 ((UINTN)Address.IoAddress32);
  } else {
    // Read 8-bit value from 16-bit I/O address
    Value = IoRead8 ((UINTN)Address.IoAddress16);
  }

  return Value;
}

/**
  This function writes 8-bit value to register address.

  @param[in]      Address               This represents either 16-bit IO address
                                        or 32-bit memory mapped address.
  @param[in]      Value                 8-bit value write to register address

**/
VOID
KcsRegisterWrite8 (
  MANAGEABILITY_TRANSPORT_HARDWARE_IO  Address,
  UINT8                                Value
  )
{
  if (mKcsHardwareInfo.MemoryMap == MANAGEABILITY_TRANSPORT_KCS_MEMORY_MAP_IO) {
    // Write 8-bit value to 32-bit Memory mapped address.
    MmioWrite8 ((UINTN)Address.IoAddress32, Value);
  } else {
    // Write 8-bit value to 16-bit I/O address
    IoWrite8 ((UINTN)Address.IoAddress16, Value);
  }
}
