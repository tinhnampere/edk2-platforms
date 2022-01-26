/** @file

  Copyright (c) 2022, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Guid/GlobalVariable.h>
#include <Library/ArmLib.h>
#include <Library/BaseCryptLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/SecVarLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Protocol/MmCommunication2.h>
#include <Uefi/UefiMultiPhase.h>
#include <Uefi/UefiBaseType.h>
#include "SecVarLibCommon.h"

#define MM_SECVAR_PAYLOAD_LENGTH  5
#define MM_BUF_SIZE               0x100

#define AUTHINFO_2_SIZE(Cert) \
          (((UINTN)(((EFI_VARIABLE_AUTHENTICATION_2 *)(Cert))->AuthInfo.Hdr.dwLength)) + sizeof (EFI_TIME))

#define MAX_SIG_BUF_SIZE   (1*1024)
#define MAX_NAME_BUF_SIZE  (256)
#define MAX_CERT_BUF_SIZE  (3*1024)
#define SMC_VAR_MAX_SIZE   (64*1024)

#pragma pack(1)
typedef struct {
  UINT16      Slot;
  UINT16      Reserve1;
  UINT32      Reserve2;
  UINT32      NameLen;
  UINT32      Reserve3;
  VOID        *Name;
  EFI_GUID    VendorGuid;
  UINT32      Attributes;
  UINT32      Reserve4;
  EFI_TIME    TimeStamp;
  VOID        *SignerCert;
  UINT32      SignerCertLen;
  UINT32      Reserve5;
  VOID        *Signature;
  UINT32      SignatureLen;
  UINT32      Reserve6;
} ARM_SMC_KEY_INFO;

/**
  Key slot stucture data
**/
typedef struct {
  CHAR16      *VariableName;        ///< Name of key
  EFI_GUID    *VendorGuid;          ///< GUID of key
  UINTN       Slot;                 ///< Key's handle id
  VOID        *Data;                ///< Pointer to key data
} KEY_SLOT;
#pragma pack()

STATIC KEY_SLOT  Key2SMCSlot[] = {
  { AMPERE_FWA_CERT_NAME, &gAmpereCertVendorGuid, 8, NULL },
  { AMPERE_FWU_CERT_NAME, &gAmpereCertVendorGuid, 9, NULL },
  { NULL,                 NULL,                   0, NULL }
};

EFI_MM_COMMUNICATION2_PROTOCOL  *mSecDxeMmCommunication = NULL;
UINT8                           *mCommBuffer = NULL;

STATIC UINT8             *mSignatureBuf = NULL;
STATIC UINT8             *mNameBuf    = NULL;
STATIC UINT8             *gCertBuf    = NULL;
STATIC UINT8             *mDataBuf    = NULL;
STATIC ARM_SMC_KEY_INFO  *mSmcKeyInfo = NULL;

/**
  This function returns UNICODE string size in bytes

  @param[in] String - Pointer to string

  @retval UINTN - Size of string in bytes excluding the null-terminator

**/
STATIC
UINTN
StrSize16 (
  IN CHAR16 *String
  )
{
  UINTN  Size = 0;

  while (*String++) {
    Size += 2;
  }

  return Size;
}

/**
  This function lookups KeySlot struct base on VariableName and  VendorGuid

  @param[in] VariableName - Pointer to variable name
  @param[in] VendorGuid   - Pointer to GUID of variable
  @param[out] KeySlot     - Pointer to KeySlot structure

**/
STATIC
VOID
SecurityKeyLookUp (
  IN CHAR16    *VariableName,
  IN EFI_GUID  *VendorGuid,
  OUT KEY_SLOT **KeySlot
  )
{
  INTN  Idx;

  ASSERT (VariableName != NULL);
  ASSERT (VendorGuid != NULL);

  for (Idx = 0; Key2SMCSlot[Idx].VariableName != NULL; Idx++) {
    if ((StrCmp (VariableName, Key2SMCSlot[Idx].VariableName) == 0) &&
        CompareGuid (VendorGuid, Key2SMCSlot[Idx].VendorGuid))
    {
      *KeySlot = &Key2SMCSlot[Idx];
      break;
    }
  }
}

/**
  This function create SecVar MM request buffer

  @param[in,out]  Data - Pointer to buffer data
  @param[in]      Size - Size of buffer

**/
STATIC
EFI_STATUS
UefiMmCreateSecVarReq (
  IN OUT  VOID   *Data,
  IN      UINT64 Size
  )
{
  EFI_MM_COMM_REQUEST  *EfiMmSecVarReq;

  EfiMmSecVarReq = (EFI_MM_COMM_REQUEST *)mCommBuffer;

  CopyGuid (&EfiMmSecVarReq->EfiMmHdr.HeaderGuid, &gSecVarMmGuid);
  EfiMmSecVarReq->EfiMmHdr.MsgLength = Size;

  if ((Size > EFI_MM_MAX_PAYLOAD_SIZE) || (Data == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  CopyMem (EfiMmSecVarReq->PayLoad.Data, Data, Size);

  return EFI_SUCCESS;
}

/**
  This function perform SecVar MM get variable

  @param[in] KeyInfo    - Pointer to SecVar Key info
  @param[in] Attributes - Pointer to return attributes of variable,
  @param[out] Data      - Pointer to return variable data
  @param[out] Len       - Pointer to return data len of secure variable

**/
STATIC
EFI_STATUS
SecurityKeyGet (
  IN  ARM_SMC_KEY_INFO *KeyInfo,
  IN  UINT32           *Attributes OPTIONAL,
  OUT VOID             *Data,
  OUT UINTN            *Len
  )
{
  EFI_MM_COMM_REQUEST            *EfiMmSecVarReq;
  EFI_MM_COMMUNICATE_SECVAR_RES  *MmSecVarRes;
  EFI_STATUS                     Status;
  UINT64                         MmData[MM_SECVAR_PAYLOAD_LENGTH];
  UINTN                          Size;

  ZeroMem (MmData, sizeof (MmData));
  MmData[0] = MM_SECVAR_KEYGET;
  MmData[1] = (UINT64)mSmcKeyInfo;
  MmData[2] = (UINT64)mDataBuf;
  MmData[3] = SMC_VAR_MAX_SIZE;

  if ((KeyInfo == NULL) || (Data == NULL) || (Len == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  CopyMem (mSmcKeyInfo, KeyInfo, sizeof (*mSmcKeyInfo));

  Status = UefiMmCreateSecVarReq ((VOID *)&MmData, sizeof (MmData));
  if (EFI_ERROR (Status)) {
    return EFI_INVALID_PARAMETER;
  }

  Size   = sizeof (EFI_MM_COMM_HEADER_NOPAYLOAD) + sizeof (MmData);
  Status = mSecDxeMmCommunication->Communicate (
                                     mSecDxeMmCommunication,
                                     mCommBuffer,
                                     mCommBuffer,
                                     &Size
                                     );
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  /* Return data in payload */
  EfiMmSecVarReq = (EFI_MM_COMM_REQUEST *)mCommBuffer;
  MmSecVarRes    = (EFI_MM_COMMUNICATE_SECVAR_RES *)&EfiMmSecVarReq->PayLoad;
  if (MmSecVarRes->Status != MM_SECVAR_RES_SUCCESS) {
    switch (MmSecVarRes->Status) {
      case MM_SECVAR_RES_NOT_SET:
        return EFI_NOT_FOUND;

      case MM_SECVAR_RES_INSUFFICIENT_RES:
        return EFI_BUFFER_TOO_SMALL;

      case MM_SECVAR_RES_IO_ERROR:
        return EFI_DEVICE_ERROR;

      default:
        return EFI_INVALID_PARAMETER;
    }
  }

  if (*Len < MmSecVarRes->Len) {
    return EFI_BUFFER_TOO_SMALL;
  }

  if (MmSecVarRes->Len > SMC_VAR_MAX_SIZE) {
    return EFI_DEVICE_ERROR;
  }

  *Len = MmSecVarRes->Len;
  if (Attributes != NULL) {
    *Attributes = MmSecVarRes->Attr;
  }

  CopyMem (Data, mDataBuf, *Len);

  return EFI_SUCCESS;
}

/**
  This function perform SecVar MM set variable

  @param[in] KeyInfo   - Pointer to SecVar Key info
  @param[in] Data      - Pointer to secure variable data
  @param[in] Len       - Pointer to data len of secure variable

**/
STATIC
EFI_STATUS
SecurityKeySet (
  IN ARM_SMC_KEY_INFO *KeyInfo,
  IN VOID             *Data,
  IN UINTN            Len
  )
{
  EFI_MM_COMM_REQUEST            *EfiMmCommSecVarReq;
  EFI_MM_COMMUNICATE_SECVAR_RES  *MmSecVarRes;
  EFI_STATUS                     Status;
  UINT64                         MmData[MM_SECVAR_PAYLOAD_LENGTH];
  UINTN                          Size;

  if ((KeyInfo == NULL) || (Len > SMC_VAR_MAX_SIZE) ||
      (KeyInfo->NameLen > MAX_NAME_BUF_SIZE) ||
      (KeyInfo->SignatureLen > MAX_SIG_BUF_SIZE) ||
      (KeyInfo->SignerCertLen > MAX_CERT_BUF_SIZE))
  {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (MmData, sizeof (MmData));
  MmData[0] = MM_SECVAR_KEYSET;
  MmData[1] = (UINT64)mSmcKeyInfo;
  if (Data == NULL) {
    MmData[2] = 0;
    MmData[3] = 0;
  } else {
    MmData[2] = (UINT64)mDataBuf;
    MmData[3] = Len;
    CopyMem (mDataBuf, Data, Len);
  }

  CopyMem (mSmcKeyInfo, KeyInfo, sizeof (*mSmcKeyInfo));

  if (KeyInfo->SignerCert != NULL) {
    CopyMem (gCertBuf, KeyInfo->SignerCert, KeyInfo->SignerCertLen);
    mSmcKeyInfo->SignerCert = gCertBuf;
  }

  if (KeyInfo->Signature != NULL) {
    CopyMem (mSignatureBuf, KeyInfo->Signature, KeyInfo->SignatureLen);
    mSmcKeyInfo->Signature = mSignatureBuf;
  }

  if (KeyInfo->Name != NULL) {
    CopyMem (mNameBuf, KeyInfo->Name, KeyInfo->NameLen);
    mSmcKeyInfo->Name = mNameBuf;
  }

  Status = UefiMmCreateSecVarReq ((VOID *)&MmData, sizeof (MmData));
  if (EFI_ERROR (Status)) {
    return EFI_INVALID_PARAMETER;
  }

  Size = sizeof (EFI_MM_COMM_HEADER_NOPAYLOAD) + sizeof (MmData);

  Status = mSecDxeMmCommunication->Communicate (
                                     mSecDxeMmCommunication,
                                     mCommBuffer,
                                     mCommBuffer,
                                     &Size
                                     );
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  /* Return data in payload */
  EfiMmCommSecVarReq = (EFI_MM_COMM_REQUEST *)mCommBuffer;
  MmSecVarRes = (EFI_MM_COMMUNICATE_SECVAR_RES *)&EfiMmCommSecVarReq->PayLoad;
  if (MmSecVarRes->Status != MM_SECVAR_RES_SUCCESS) {
    if ((MmSecVarRes->Status == MM_SECVAR_RES_ACCESS_DENIED) ||
        (MmSecVarRes->Status == MM_SECVAR_RES_AUTH_ERROR))
    {
      return EFI_SECURITY_VIOLATION;
    }

    if (MmSecVarRes->Status == MM_SECVAR_RES_INSUFFICIENT_RES) {
      return EFI_OUT_OF_RESOURCES;
    }

    if (MmSecVarRes->Status == MM_SECVAR_RES_IO_ERROR) {
      return EFI_DEVICE_ERROR;
    }

    if (MmSecVarRes->Status == MM_SECVAR_RES_NOT_SET) {
      return EFI_NOT_FOUND;
    }

    return EFI_INVALID_PARAMETER;
  }

  return EFI_SUCCESS;
}

/**
  Enables secure variable authentication feature.

  @retval EFI_SUCCESS               Update operation success
  @retval EFI_DEVICE_ERROR          Not enough resource
 **/
EFI_STATUS
SecVarEnableKeyAuth (
  VOID
  )
{
  EFI_MM_COMM_REQUEST            *EfiMmSecVarReq;
  EFI_MM_COMMUNICATE_SECVAR_RES  *MmSecVarRes;
  EFI_STATUS                     Status;
  UINT64                         MmData[MM_SECVAR_PAYLOAD_LENGTH];
  UINTN                          Size;

  MmData[0] = MM_SECVAR_AUTHEN;

  Status = UefiMmCreateSecVarReq ((VOID *)&MmData, sizeof (MmData));
  if (EFI_ERROR (Status)) {
    return EFI_INVALID_PARAMETER;
  }

  Size   = sizeof (EFI_MM_COMM_HEADER_NOPAYLOAD) + sizeof (MmData);
  Status = mSecDxeMmCommunication->Communicate (
                                     mSecDxeMmCommunication,
                                     mCommBuffer,
                                     mCommBuffer,
                                     &Size
                                     );
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  /* Return data in payload */
  EfiMmSecVarReq = (EFI_MM_COMM_REQUEST *)mCommBuffer;
  MmSecVarRes    = (EFI_MM_COMMUNICATE_SECVAR_RES *)&EfiMmSecVarReq->PayLoad;
  if (MmSecVarRes->Status != MM_SECVAR_RES_FAIL) {
    return EFI_DEVICE_ERROR;
  }

  return EFI_SUCCESS;
}

/**
  Get secure variable via MM Interface.

  @param[in]  VariableName          Name of the variable
  @param[in]  VendorGuid            Variable vendor GUID
  @param[out] Attributes            Pointer to attributes address
  @param[out] Data                  Pointer to data address
  @param[out] DataSize              Pointer to data size

  @retval EFI_INVALID_PARAMETER     One or more parameter invalid
  @retval EFI_SUCCESS               Variable query successfully
  @retval EFI_NOT_FOUND             Variable not found
**/
EFI_STATUS
SecVarGetSecureVariable (
  IN  CHAR16   *VariableName,
  IN  EFI_GUID *VendorGuid,
  OUT UINT32   *Attributes OPTIONAL,
  OUT VOID     **Data,
  OUT UINTN    *DataSize
  )
{
  ARM_SMC_KEY_INFO  KeyInfo;
  KEY_SLOT          *KeySlot;
  EFI_STATUS        Status;
  UINTN             OriginalDataSize = 0;

  //
  // Data can be NULL, which means the caller is querying the size.
  // Attributes can be NULL but if provided, will be returned.
  if ((VariableName == NULL) || (VendorGuid == NULL) || (DataSize == NULL)) {
    DEBUG ((
      DEBUG_ERROR,
      "SecVar KeyGet failed. VariableName %p VendorGuid %p DataSize %p\n",
      VariableName,
      VendorGuid,
      DataSize
      ));
    return EFI_INVALID_PARAMETER;
  }

  OriginalDataSize = *DataSize;

  //
  // Check if the variable name match our expected variable
  // This must be the first check since we can send the checks for
  // unsupported variable names and guids to other functions.
  SecurityKeyLookUp (VariableName, VendorGuid, &KeySlot);
  if (KeySlot == NULL) {
    DEBUG ((DEBUG_ERROR, "SecVar KeyGet. Attempting to read unsupported guid.\n"));
    return EFI_UNSUPPORTED;
  }

  // If data is NULL and *DataSize is not zero, it is an error.
  // Data can be NULL if *DataSize is 0, in which case the required
  // size of the buffer is returned.
  if ((Data == NULL) && (*DataSize != 0)) {
    DEBUG ((DEBUG_ERROR, "SecVar KeyGet failed. Data NULL Data Size not 0\n"));
    return EFI_INVALID_PARAMETER;
  }

  if (KeySlot->Data == NULL) {
    DEBUG ((DEBUG_ERROR, "SecVar KeyGet: No memory allocated for data.\n"));
    return EFI_OUT_OF_RESOURCES;
  }

  KeyInfo.Slot = KeySlot->Slot;
  *DataSize    = SMC_VAR_MAX_SIZE;
  Status = SecurityKeyGet (&KeyInfo, Attributes, KeySlot->Data, DataSize);
  if (EFI_ERROR (Status)) {
    *DataSize = 0;
    DEBUG ((DEBUG_ERROR, "SecVar KeyGet %s failed %d\n", VariableName, Status));
  } else {
    if ((Data == NULL) || (0 == OriginalDataSize) ||
        (OriginalDataSize < *DataSize))
    {
      DEBUG ((
        DEBUG_INFO,
        "SecVar KeyGet Returning Data Size only, since Data is NULL or DataSize is 0 or "
        "buffer size is too small.\n"
        ));
      return EFI_BUFFER_TOO_SMALL;
    }

    *Data = KeySlot->Data;
  }

  return Status;
}

/**
  Set/Update secure variable via MM Interface.

  @param[in] VariableName           Name of variable
  @param[in] VendorGuid             Guid of variable
  @param[in] Data                   Data pointer
  @param[in] DataSize               Size of Data

  @retval EFI_SUCCESS               Update operation succeed
  @retval EFI_INVALID_PARAMETER     Invalid parameter
  @retval EFI_WRITE_PROTECTED       Wite-protected variable
  @retval EFI_OUT_OF_RESOURCES      Not enough resource

**/
EFI_STATUS
SecVarSetSecureVariable (
  IN CHAR16   *VariableName,
  IN EFI_GUID *VendorGuid,
  IN UINT32   Attributes,
  IN VOID     *Data,
  IN UINTN    DataSize
  )
{
  EFI_VARIABLE_AUTHENTICATION_2  *CertData;
  ARM_SMC_KEY_INFO               KeyInfo;
  KEY_SLOT                       *KeySlot;
  EFI_STATUS                     Status;
  UINTN                          CertHdrSize;
  UINTN                          Pkcs7CertLen;
  UINT8                          *Pkcs7Cert;
  UINT8                          *CertBuffer     = NULL;
  UINTN                          BufferLength    = 0;
  UINT8                          *SignerCert     = NULL;
  UINTN                          SignerCertLen   = 0;
  UINT8                          *Signature      = NULL;
  UINTN                          SignatureLen    = 0;
  UINT8                          *VarDataStart   = NULL;
  UINTN                          VarDataLen      = 0;
  BOOLEAN                        IgnoreSignature = FALSE;

  if ((VariableName == NULL) || (VendorGuid == NULL) || (Data == NULL)) {
    DEBUG ((
      DEBUG_ERROR,
      "SecVar KeyGet failed. VariableName %p VendorGuid %p\n",
      VariableName,
      VendorGuid
      ));
    return EFI_INVALID_PARAMETER;
  }

  //
  // Check if the variable name match our expected variable
  // This must be the first check since we can send the checks for
  // unsupported variable names and guids to other functions.
  SecurityKeyLookUp (VariableName, VendorGuid, &KeySlot);
  if (KeySlot == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "SecVar KeyGet. Attempting to set unsupported guid %p.\n",
      VendorGuid
      ));
    return EFI_UNSUPPORTED;
  }

  if (Data != NULL) {
    if (KeySlot->Data == NULL) {
      DEBUG ((DEBUG_ERROR, "SecVar KeyGet No memory allocated for data.\n"));
      return EFI_OUT_OF_RESOURCES;
    }

    // Process the secure variable
    CertData    = (EFI_VARIABLE_AUTHENTICATION_2 *)Data;
    CertHdrSize = AUTHINFO_2_SIZE (CertData);
    if (DataSize < CertHdrSize) {
      DEBUG ((DEBUG_ERROR, "SecVar KeyGet Invalid Data Size\n"));
      return EFI_SECURITY_VIOLATION;
    }

    //
    // CertBlock->CerData is a beginning of Pkcs7 Cert
    Pkcs7Cert    = (UINT8 *)&(CertData->AuthInfo.CertData);
    Pkcs7CertLen = CertData->AuthInfo.Hdr.dwLength - (UINT32)(OFFSET_OF (WIN_CERTIFICATE_UEFI_GUID, CertData));

    //
    // wCertificateType should be WIN_CERT_TYPE_EFI_GUID.
    // Cert type should be EFI_CERT_TYPE_PKCS7_GUID.
    if ((CertData->AuthInfo.Hdr.wCertificateType != WIN_CERT_TYPE_EFI_GUID) ||
        !CompareGuid ((EFI_GUID *)&(CertData->AuthInfo.CertType), &gEfiCertPkcs7Guid))
    {
      //
      // Invalid AuthInfo type, return EFI_SECURITY_VIOLATION.
      DEBUG ((DEBUG_ERROR, "SecVar KeyGet Invalid AuthInfo type\n"));
      return EFI_SECURITY_VIOLATION;
    }

    //
    // Retrive the CA key
    Status = Pkcs7GetSigners (
               Pkcs7Cert,
               Pkcs7CertLen,                            // Pkcs7Cert
               &CertBuffer,
               &BufferLength,                             // In/OutData
               &SignerCert,
               &SignerCertLen
               );

    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_INFO, "SecVar KeyGet failed to get signer cert. Ignoring signature.\n"));
      //
      // If we dont find a signature in the PKCS7Cert, ignore the
      // signature and attempt to set the variable anyway. If authentication
      // is disabled(usually when ExitBootServices() has not yet been called)
      // the variable set may still succeed.
      IgnoreSignature = TRUE;
    }

    if (FALSE == IgnoreSignature) {
      Status = Pkcs7GetSignature (
                 Pkcs7Cert,
                 Pkcs7CertLen,
                 NULL,
                 &SignatureLen
                 );

      if (EFI_ERROR (Status) || (SignatureLen == 0)) {
        DEBUG ((DEBUG_ERROR, "SecVar KeyGet failed to get signature length\n"));
        return Status;
      }

      Signature = mSignatureBuf;
      if ((Signature == NULL) || (SignatureLen > MAX_SIG_BUF_SIZE)) {
        DEBUG ((DEBUG_ERROR, "SecVar KeyGet Not enough memory allocated for Signature.\n"));
        return EFI_OUT_OF_RESOURCES;
      }

      Status = Pkcs7GetSignature (
                 Pkcs7Cert,
                 Pkcs7CertLen,                            // Pkcs7Cert
                 &Signature,
                 &SignatureLen
                 );

      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "SecVar KeyGet failed to get signature\n"));
        return Status;
      }
    }

    if (DataSize > CertHdrSize) {
      VarDataStart = (UINT8 *)Data + (CertHdrSize);
      VarDataLen   = DataSize - (CertHdrSize);
    }
  } else {
    SignerCert    = NULL;
    SignerCertLen = 0;
    Signature     = NULL;
    SignatureLen  = 0;
  }

  KeyInfo.Slot       = KeySlot->Slot;
  KeyInfo.NameLen    = StrSize16 (VariableName);
  KeyInfo.Name       = VariableName;
  KeyInfo.VendorGuid = *VendorGuid;
  KeyInfo.Attributes = Attributes;
  if (Data != NULL) {
    KeyInfo.TimeStamp = CertData->TimeStamp;
  } else {
    ZeroMem (&(KeyInfo.TimeStamp), sizeof (KeyInfo.TimeStamp));
  }

  KeyInfo.SignerCert    = SignerCert;
  KeyInfo.SignerCertLen = SignerCertLen;
  KeyInfo.Signature     = Signature;
  KeyInfo.SignatureLen  = SignatureLen;
  Status = SecurityKeySet (&KeyInfo, VarDataStart, VarDataLen);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "SecVar KeyGet %s failed %d\n", VariableName, Status));
    if (!Data && !DataSize) {
      Status = EFI_SUCCESS;
    }
  }

  Pkcs7FreeSigners (CertBuffer);
  Pkcs7FreeSigners (SignerCert);
  return EFI_SUCCESS;
}

/**
  This function free library resources

 **/
STATIC
VOID
SerVarLibFreeResources (
  VOID
  )
{
  UINTN     Idx = 0;
  KEY_SLOT  *KeySlot;

  for (Idx = 0; Key2SMCSlot[Idx].VariableName != NULL; Idx++) {
    KeySlot = &Key2SMCSlot[Idx];
    if (KeySlot->Data != NULL) {
      FreePool (KeySlot->Data);
    }
  }

  if (mSignatureBuf != NULL) {
    FreePool (mSignatureBuf);
  }

  if (mDataBuf != NULL) {
    FreePool (mDataBuf);
  }

  if (mSmcKeyInfo != NULL) {
    FreePool (mSmcKeyInfo);
  }

  if (mNameBuf != NULL) {
    FreePool (mNameBuf);
  }

  if (gCertBuf != NULL) {
    FreePool (gCertBuf);
  }

  if (mCommBuffer != NULL) {
    FreePool (mCommBuffer);
  }
}

/**
  Constructor function of the SerVarLib.

  @param ImageHandle        The image handle.
  @param SystemTable        The system table.

  @retval  EFI_SUCCESS      Operation succeeded.
  @retval  Others           An error has occurred
**/
EFI_STATUS
EFIAPI
SerVarLibConstructor (
  IN EFI_HANDLE       ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
  )
{
  UINTN       Idx = 0;
  KEY_SLOT    *KeySlot;
  EFI_STATUS  Status;

  for (Idx = 0; Key2SMCSlot[Idx].VariableName != NULL; Idx++) {
    KeySlot = &Key2SMCSlot[Idx];
    KeySlot->Data = AllocateZeroPool (SMC_VAR_MAX_SIZE);
    if (KeySlot->Data == NULL) {
      DEBUG ((
        DEBUG_ERROR,
        "%a Failed to allocate memory for keyslot data.\n",
        __FUNCTION__
        ));
      Status = EFI_OUT_OF_RESOURCES;
      goto exit;
    }
  }

  mSignatureBuf = AllocateZeroPool (MAX_SIG_BUF_SIZE);
  if (mSignatureBuf == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a Failed to allocate memory for signature buffer.\n",
      __FUNCTION__
      ));
    Status = EFI_OUT_OF_RESOURCES;
    goto exit;
  }

  mDataBuf = AllocateZeroPool (SMC_VAR_MAX_SIZE);
  if (mDataBuf == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a Failed to allocate memory for data buffer.\n",
      __FUNCTION__
      ));
    Status = EFI_OUT_OF_RESOURCES;
    goto exit;
  }

  mSmcKeyInfo = AllocateZeroPool (sizeof (*mSmcKeyInfo));
  if (mSmcKeyInfo == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a Failed to allocate memory for SmcKeyInfo buffer.\n",
      __FUNCTION__
      ));
    Status = EFI_OUT_OF_RESOURCES;
    goto exit;
  }

  mNameBuf = AllocateZeroPool (MAX_NAME_BUF_SIZE);
  if (mNameBuf == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a Failed to allocate memory for Name buffer.\n",
      __FUNCTION__
      ));
    Status = EFI_OUT_OF_RESOURCES;
    goto exit;
  }

  gCertBuf = AllocateZeroPool (MAX_CERT_BUF_SIZE);
  if (gCertBuf == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a Failed to allocate memory for certificate buffer.\n",
      __FUNCTION__
      ));
    Status = EFI_OUT_OF_RESOURCES;
    goto exit;
  }

  mCommBuffer = AllocateZeroPool (MM_BUF_SIZE);
  if (mCommBuffer == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a Failed to allocate memory for Mm buffer.\n",
      __FUNCTION__
      ));
    Status = EFI_OUT_OF_RESOURCES;
    goto exit;
  }

  Status = gBS->LocateProtocol (
                  &gEfiMmCommunication2ProtocolGuid,
                  NULL,
                  (VOID **)&mSecDxeMmCommunication
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Can't locate gEfiMmCommunicationProtocolGuid\n", __FUNCTION__));
    goto exit;
  }

exit:
  if (EFI_ERROR (Status)) {
    SerVarLibFreeResources ();
  }

  return Status;
}
