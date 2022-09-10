/** @file

  Copyright (c) 2020 - 2022, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>

#include <CpuConfigNVDataStruc.h>
#include <Guid/SmBios.h>
#include <Library/AmpereCpuLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/IOExpanderLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/NVParamLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <NVParamDef.h>
#include <Protocol/Smbios.h>

#define TYPE8_ADDITIONAL_STRINGS      \
  "VGA1 - Rear VGA Connector\0"       \
  "DB-15 Male (VGA)         \0"

#define TYPE9_ADDITIONAL_STRINGS       \
  "Socket 0 Riser 1 x32 - Slot 1\0"

#define  RISER_PRESENT      0

//
// Type7 SLC Data
//
#define MAX_CACHE_LEVEL 2

#define CACHE_SOCKETED_SHIFT        3
#define CACHE_LOCATION_SHIFT        5
#define CACHE_ENABLED_SHIFT         7
#define CACHE_OPERATION_MODE_SHIFT  8

#define SLC_SIZE(x)                 (UINT16)(0x8000 | (((x) * (1 << 20)) / (64 * (1 << 10))))
#define SLC_SIZE_2(x)               (0x80000000 | (((x) * (1 << 20)) / (64 * (1 << 10))))

typedef enum {
  CacheModeWriteThrough = 0,  ///< Cache is write-through
  CacheModeWriteBack,         ///< Cache is write-back
  CacheModeVariesWithAddress, ///< Cache mode varies by address
  CacheModeUnknown,           ///< Cache mode is unknown
  CacheModeMax
} CACHE_OPERATION_MODE;

typedef enum {
  CacheLocationInternal = 0, ///< Cache is internal to the processor
  CacheLocationExternal,     ///< Cache is external to the processor
  CacheLocationReserved,     ///< Reserved
  CacheLocationUnknown,      ///< Cache location is unknown
  CacheLocationMax
} CACHE_LOCATION;

//
// IO Expander Assignment
//
#define S0_RISERX32_SLOT1_PRESENT_PIN1    12 /* P12: S0_PCIERCB_3A_PRSNT_1C_L */
#define S0_RISERX32_SLOT1_PRESENT_PIN2    13 /* P13: S0_PCIERCB_3A_PRSNT_2C_L */
#define S0_RISERX32_SLOT2_PRESENT_PIN1    4  /* P04: S0_PCIERCA3_PRSNT_1C_L   */
#define S0_RISERX32_SLOT2_PRESENT_PIN2    5  /* P05: S0_PCIERCA3_PRSNT_2C_L   */
#define S0_RISERX32_SLOT2_PRESENT_PIN3    6  /* P06: S0_PCIERCA3_PRSNT_4C_L   */
#define S0_RISERX32_SLOT3_PRESENT_PIN1    10 /* P10: S0_PCIERCB_2B_PRSNT_1C_L */
#define S0_RISERX32_SLOT3_PRESENT_PIN2    11 /* P11: S0_PCIERCB_2B_PRSNT_2C_L */
#define S1_RISERX24_SLOT1_PRESENT_PIN1    0  /* P00: S1_PCIERCB1B_PRSNT_L     */
#define S1_RISERX24_SLOT1_PRESENT_PIN2    1  /* P01: S1_PCIERCB1B_PRSNT_1C_L  */
#define S1_RISERX24_SLOT2_PRESENT_PIN     3  /* P03: S1_PCIERCA3_2_PRSNT_4C_L */
#define S1_RISERX24_SLOT3_PRESENT_PIN     2  /* P02: S1_PCIERCA3_1_PRSNT_2C_L */
#define S1_RISERX8_SLOT1_PRESENT_PIN1     4  /* P04: S1_PCIERCB0A_PRSNT_2C_L  */
#define S1_RISERX8_SLOT1_PRESENT_PIN2     5  /* P05: S1_PCIERCB0A_PRSNT_L     */
#define S0_OCP_SLOT_PRESENT_PIN1          0  /* P00: OCP_PRSNTB0_L            */
#define S0_OCP_SLOT_PRESENT_PIN2          1  /* P01: OCP_PRSNTB1_L            */
#define S0_OCP_SLOT_PRESENT_PIN3          2  /* P02: OCP_PRSNTB2_L            */
#define S0_OCP_SLOT_PRESENT_PIN4          3  /* P03: OCP_PRSNTB3_D            */

//
// CPU I2C Bus for IO Expander
//
#define S0_RISER_I2C_BUS                  0x02
#define S0_OCP_I2C_BUS                    0x02
#define S1_RISER_I2C_BUS                  0x03

//
// I2C address of IO Expander devices
//
#define S0_RISERX32_I2C_ADDRESS           0x22
#define S1_RISERX24_I2C_ADDRESS           0x22
#define S1_RISERX8_I2C_ADDRESS            0x22
#define S0_OCP_I2C_ADDRESS                0x20

#define TYPE11_ADDITIONAL_STRINGS       \
  "www.amperecomputing.com\0"

#define TYPE41_ADDITIONAL_STRINGS       \
  "Onboard VGA\0"

#define ADDITIONAL_STR_INDEX_1    0x01
#define ADDITIONAL_STR_INDEX_2    0x02
#define ADDITIONAL_STR_INDEX_3    0x03
#define ADDITIONAL_STR_INDEX_4    0x04
#define ADDITIONAL_STR_INDEX_5    0x05
#define ADDITIONAL_STR_INDEX_6    0x06

//
// Type definition and contents of the default SMBIOS table.
// This table covers only the minimum structures required by
// the SMBIOS specification (section 6.2, version 3.0)
//
#pragma pack(1)
typedef struct {
  SMBIOS_TABLE_TYPE8 Base;
  CHAR8              Strings[sizeof (TYPE8_ADDITIONAL_STRINGS)];
} ARM_TYPE8;

typedef struct {
  SMBIOS_TABLE_TYPE9 Base;
  CHAR8              Strings[sizeof (TYPE9_ADDITIONAL_STRINGS)];
} ARM_TYPE9;

typedef struct {
  SMBIOS_TABLE_TYPE11 Base;
  CHAR8               Strings[sizeof (TYPE11_ADDITIONAL_STRINGS)];
} ARM_TYPE11;

typedef struct {
  SMBIOS_TABLE_TYPE41 Base;
  CHAR8               Strings[sizeof (TYPE41_ADDITIONAL_STRINGS)];
} ARM_TYPE41;

#pragma pack()

// Type 8 Port Connector Information
STATIC CONST ARM_TYPE8 mArmDefaultType8Vga = {
  {
    {                                             // SMBIOS_STRUCTURE Hdr
      EFI_SMBIOS_TYPE_PORT_CONNECTOR_INFORMATION, // UINT8 Type
      sizeof (SMBIOS_TABLE_TYPE8),                // UINT8 Length
      SMBIOS_HANDLE_PI_RESERVED,
    },
    ADDITIONAL_STR_INDEX_1,       // InternalReferenceDesignator String
    PortConnectorTypeDB15Female,  // InternalConnectorType;
    ADDITIONAL_STR_INDEX_2,       // ExternalReferenceDesignator String
    PortTypeOther,                // ExternalConnectorType;
    PortTypeVideoPort,            // PortType;
  },
  "VGA1 - Rear VGA Connector\0" \
  "DB-15 Male (VGA)\0"
};

// Type 8 Port Connector Information
STATIC CONST ARM_TYPE8 mArmDefaultType8USBFront = {
  {
    {                                             // SMBIOS_STRUCTURE Hdr
      EFI_SMBIOS_TYPE_PORT_CONNECTOR_INFORMATION, // UINT8 Type
      sizeof (SMBIOS_TABLE_TYPE8),                // UINT8 Length
      SMBIOS_HANDLE_PI_RESERVED,
    },
    ADDITIONAL_STR_INDEX_1,       // InternalReferenceDesignator String
    PortConnectorTypeUsb,         // InternalConnectorType;
    ADDITIONAL_STR_INDEX_2,       // ExternalReferenceDesignator String
    PortTypeOther,                // ExternalConnectorType;
    PortTypeUsb,                  // PortType;
  },
  "Front Panel USB 3.0\0"  \
  "USB\0"
};

// Type 8 Port Connector Information
STATIC CONST ARM_TYPE8 mArmDefaultType8USBRear = {
  {
    {                                             // SMBIOS_STRUCTURE Hdr
      EFI_SMBIOS_TYPE_PORT_CONNECTOR_INFORMATION, // UINT8 Type
      sizeof (SMBIOS_TABLE_TYPE8),                // UINT8 Length
      SMBIOS_HANDLE_PI_RESERVED,
    },
    ADDITIONAL_STR_INDEX_1,       // InternalReferenceDesignator String
    PortConnectorTypeUsb,         // InternalConnectorType;
    ADDITIONAL_STR_INDEX_2,       // ExternalReferenceDesignator String
    PortTypeOther,                // ExternalConnectorType;
    PortTypeUsb,                  // PortType;
  },
  "Rear Panel USB 3.0\0"   \
  "USB\0"
};

// Type 8 Port Connector Information
STATIC CONST ARM_TYPE8 mArmDefaultType8NetRJ45 = {
  {
    {                                             // SMBIOS_STRUCTURE Hdr
      EFI_SMBIOS_TYPE_PORT_CONNECTOR_INFORMATION, // UINT8 Type
      sizeof (SMBIOS_TABLE_TYPE8),                // UINT8 Length
      SMBIOS_HANDLE_PI_RESERVED,
    },
    ADDITIONAL_STR_INDEX_1,       // InternalReferenceDesignator String
    PortConnectorTypeRJ45,        // InternalConnectorType;
    ADDITIONAL_STR_INDEX_2,       // ExternalReferenceDesignator String
    PortConnectorTypeRJ45,        // ExternalConnectorType;
    PortTypeNetworkPort,          // PortType;
  },
  "RJ1 - BMC RJ45 Port\0" \
  "RJ45 Connector\0"
};

// Type 8 Port Connector Information
STATIC CONST ARM_TYPE8 mArmDefaultType8NetOcp = {
  {
    {                                             // SMBIOS_STRUCTURE Hdr
      EFI_SMBIOS_TYPE_PORT_CONNECTOR_INFORMATION, // UINT8 Type
      sizeof (SMBIOS_TABLE_TYPE8),                // UINT8 Length
      SMBIOS_HANDLE_PI_RESERVED,
    },
    ADDITIONAL_STR_INDEX_1,       // InternalReferenceDesignator String
    PortTypeOther,                // InternalConnectorType;
    ADDITIONAL_STR_INDEX_2,       // ExternalReferenceDesignator String
    PortTypeOther,                // ExternalConnectorType;
    PortTypeNetworkPort,          // PortType;
  },
  "OCP1 - OCP NIC 3.0 Connector\0"  \
  "OCP NIC 3.0\0"
};

// Type 8 Port Connector Information
STATIC CONST ARM_TYPE8 mArmDefaultType8Uart = {
  {
    {                                             // SMBIOS_STRUCTURE Hdr
      EFI_SMBIOS_TYPE_PORT_CONNECTOR_INFORMATION, // UINT8 Type
      sizeof (SMBIOS_TABLE_TYPE8),                // UINT8 Length
      SMBIOS_HANDLE_PI_RESERVED,
    },
    ADDITIONAL_STR_INDEX_1,        // InternalReferenceDesignator String
    PortTypeOther,                 // InternalConnectorType;
    ADDITIONAL_STR_INDEX_2,        // ExternalReferenceDesignator String
    PortConnectorTypeDB9Female,    // ExternalConnectorType;
    PortTypeSerial16550Compatible, // PortType;
  },
  "UART1 - BMC UART5 Connector\0"  \
  "DB-9 female\0"
};

// Type 9 System Slots
STATIC ARM_TYPE9 mArmDefaultType9Sk0RiserX32Slot1 = {
  {
    {                               // SMBIOS_STRUCTURE Hdr
      EFI_SMBIOS_TYPE_SYSTEM_SLOTS, // UINT8 Type
      sizeof (SMBIOS_TABLE_TYPE9),  // UINT8 Length
      SMBIOS_HANDLE_PI_RESERVED,
    },
    ADDITIONAL_STR_INDEX_1,
    SlotTypePciExpressGen4,
    SlotDataBusWidth8X,
    SlotUsageAvailable,
    SlotLengthLong,
    1,         // Slot ID
    {0, 0, 1}, // Provides 3.3 Volts
    {1},       // PME
    5,
    0,
    0,
  },
  "S0 Riser x32 - Slot 1\0"
};

// Type 9 System Slots
STATIC ARM_TYPE9 mArmDefaultType9Sk0RiserX32Slot2 = {
  {
    {                               // SMBIOS_STRUCTURE Hdr
      EFI_SMBIOS_TYPE_SYSTEM_SLOTS, // UINT8 Type
      sizeof (SMBIOS_TABLE_TYPE9),  // UINT8 Length
      SMBIOS_HANDLE_PI_RESERVED,
    },
    ADDITIONAL_STR_INDEX_1,
    SlotTypePciExpressGen4,
    SlotDataBusWidth16X,
    SlotUsageUnavailable,
    SlotLengthLong,
    2,         // Slot ID
    {0, 0, 1}, // Provides 3.3 Volts
    {1},       // PME
    0,
    0,
    0,
  },
  "S0 Riser x32 - Slot 2\0"
};

// Type 9 System Slots
STATIC ARM_TYPE9 mArmDefaultType9Sk0RiserX32Slot3 = {
  {
    {                               // SMBIOS_STRUCTURE Hdr
      EFI_SMBIOS_TYPE_SYSTEM_SLOTS, // UINT8 Type
      sizeof (SMBIOS_TABLE_TYPE9),  // UINT8 Length
      SMBIOS_HANDLE_PI_RESERVED,
    },
    ADDITIONAL_STR_INDEX_1,
    SlotTypePciExpressGen4,
    SlotDataBusWidth8X,
    SlotUsageUnavailable,
    SlotLengthLong,
    3,         // Slot ID
    {0, 0, 1}, // Provides 3.3 Volts
    {1},       // PME
    4,
    0,
    0,
  },
  "S0 Riser x32 - Slot 3\0"
};

// Type 9 System Slots
STATIC ARM_TYPE9 mArmDefaultType9Sk1RiserX24Slot1 = {
  {
    {                               // SMBIOS_STRUCTURE Hdr
      EFI_SMBIOS_TYPE_SYSTEM_SLOTS, // UINT8 Type
      sizeof (SMBIOS_TABLE_TYPE9),  // UINT8 Length
      SMBIOS_HANDLE_PI_RESERVED,
    },
    ADDITIONAL_STR_INDEX_1,
    SlotTypePciExpressGen4,
    SlotDataBusWidth8X,
    SlotUsageUnavailable,
    SlotLengthLong,
    4,         // Slot ID
    {0, 0, 1}, // Provides 3.3 Volts
    {1},       // PME
    9,
    0,
    0,
  },
  "S1 Riser x24 - Slot 1\0"
};

// Type 9 System Slots
STATIC ARM_TYPE9 mArmDefaultType9Sk1RiserX24Slot2 = {
  {
    {                               // SMBIOS_STRUCTURE Hdr
      EFI_SMBIOS_TYPE_SYSTEM_SLOTS, // UINT8 Type
      sizeof (SMBIOS_TABLE_TYPE9),  // UINT8 Length
      SMBIOS_HANDLE_PI_RESERVED,
    },
    ADDITIONAL_STR_INDEX_1,
    SlotTypePciExpressGen4,
    SlotDataBusWidth8X,
    SlotUsageUnavailable,
    SlotLengthLong,
    5,         // Slot ID
    {0, 0, 1}, // Provides 3.3 Volts
    {1},       // PME
    7,
    0,
    0,
  },
  "S1 Riser x24 - Slot 2\0"
};

// Type 9 System Slots
STATIC ARM_TYPE9 mArmDefaultType9Sk1RiserX24Slot3 = {
  {
    {                               // SMBIOS_STRUCTURE Hdr
      EFI_SMBIOS_TYPE_SYSTEM_SLOTS, // UINT8 Type
      sizeof (SMBIOS_TABLE_TYPE9),  // UINT8 Length
      SMBIOS_HANDLE_PI_RESERVED,
    },
    ADDITIONAL_STR_INDEX_1,
    SlotTypePciExpressGen4,
    SlotDataBusWidth8X,
    SlotUsageUnavailable,
    SlotLengthLong,
    6,         // Slot ID
    {0, 0, 1}, // Provides 3.3 Volts
    {1},       // PME
    7,
    0,
    0,
  },
  "S1 Riser x24 - Slot 3\0"
};

// Type 9 System Slots
STATIC ARM_TYPE9 mArmDefaultType9Sk1RiserX8Slot1 = {
  {
    {                               // SMBIOS_STRUCTURE Hdr
      EFI_SMBIOS_TYPE_SYSTEM_SLOTS, // UINT8 Type
      sizeof (SMBIOS_TABLE_TYPE9),  // UINT8 Length
      SMBIOS_HANDLE_PI_RESERVED,
    },
    ADDITIONAL_STR_INDEX_1,
    SlotTypePciExpressGen4,
    SlotDataBusWidth8X,
    SlotUsageUnavailable,
    SlotLengthLong,
    7,         // Slot ID
    {0, 0, 1}, // Provides 3.3 Volts
    {1},       // PME
    8,
    0,
    0,
  },
  "S1 Riser x8 - Slot 1\0"
};

// Type 9 System Slots
STATIC ARM_TYPE9 mArmDefaultType9Sk0OcpNic = {
  {
    {                               // SMBIOS_STRUCTURE Hdr
      EFI_SMBIOS_TYPE_SYSTEM_SLOTS, // UINT8 Type
      sizeof (SMBIOS_TABLE_TYPE9),  // UINT8 Length
      SMBIOS_HANDLE_PI_RESERVED,
    },
    ADDITIONAL_STR_INDEX_1,
    SlotTypePciExpressGen4,
    SlotDataBusWidth16X,
    SlotUsageUnavailable,
    SlotLengthLong,
    8,         // Slot ID
    {0, 0, 1}, // Provides 3.3 Volts
    {1},       // PME
    1,
    0,
    0,
  },
  "S0 OCP NIC 3.0\0"
};

// Type 9 System Slots
STATIC ARM_TYPE9 mArmDefaultType9Sk0NvmeM2Slot1 = {
  {
    {                               // SMBIOS_STRUCTURE Hdr
      EFI_SMBIOS_TYPE_SYSTEM_SLOTS, // UINT8 Type
      sizeof (SMBIOS_TABLE_TYPE9),  // UINT8 Length
      SMBIOS_HANDLE_PI_RESERVED,
    },
    ADDITIONAL_STR_INDEX_1,
    SlotTypePciExpressGen4,
    SlotDataBusWidth4X,
    SlotUsageAvailable,
    SlotLengthShort,
    9,         // Slot ID
    {0, 0, 1}, // Provides 3.3 Volts
    {1},       // PME
    5,
    0,
    0,
  },
  "S1 NVMe M.2 - Slot 1\0"
};

// Type 9 System Slots
STATIC ARM_TYPE9 mArmDefaultType9Sk0NvmeM2Slot2 = {
  {
    {                               // SMBIOS_STRUCTURE Hdr
      EFI_SMBIOS_TYPE_SYSTEM_SLOTS, // UINT8 Type
      sizeof (SMBIOS_TABLE_TYPE9),  // UINT8 Length
      SMBIOS_HANDLE_PI_RESERVED,
    },
    ADDITIONAL_STR_INDEX_1,
    SlotTypePciExpressGen4,
    SlotDataBusWidth4X,
    SlotUsageAvailable,
    SlotLengthShort,
    10,         // Slot ID
    {0, 0, 1}, // Provides 3.3 Volts
    {1},       // PME
    5,
    0,
    0,
  },
  "S1 NVMe M.2 - Slot 2\0"
};

// Type 11 OEM Strings
STATIC ARM_TYPE11 mArmDefaultType11 = {
  {
    {                               // SMBIOS_STRUCTURE Hdr
      EFI_SMBIOS_TYPE_OEM_STRINGS,  // UINT8 Type
      sizeof (SMBIOS_TABLE_TYPE11), // UINT8 Length
      SMBIOS_HANDLE_PI_RESERVED,
    },
    ADDITIONAL_STR_INDEX_1
  },
  TYPE11_ADDITIONAL_STRINGS
};

// Type 24 Hardware Security
STATIC SMBIOS_TABLE_TYPE24 mArmDefaultType24 = {
  {                                    // SMBIOS_STRUCTURE Hdr
    EFI_SMBIOS_TYPE_HARDWARE_SECURITY, // UINT8 Type
    sizeof (SMBIOS_TABLE_TYPE24),      // UINT8 Length
    SMBIOS_HANDLE_PI_RESERVED,
  },
  0
};

// Type 32 System Boot Information
STATIC SMBIOS_TABLE_TYPE32 mArmDefaultType32 = {
  {                                          // SMBIOS_STRUCTURE Hdr
    EFI_SMBIOS_TYPE_SYSTEM_BOOT_INFORMATION, // UINT8 Type
    sizeof (SMBIOS_TABLE_TYPE32),            // UINT8 Length
    SMBIOS_HANDLE_PI_RESERVED,
  },
  {0, 0, 0, 0, 0, 0},
  0
};

// Type 38 IPMI Device Information
STATIC SMBIOS_TABLE_TYPE38 mArmDefaultType38 = {
  {                                          // SMBIOS_STRUCTURE Hdr
    EFI_SMBIOS_TYPE_IPMI_DEVICE_INFORMATION, // UINT8 Type
    sizeof (SMBIOS_TABLE_TYPE38),            // UINT8 Length
    SMBIOS_HANDLE_PI_RESERVED,
  },
  IPMIDeviceInfoInterfaceTypeSSIF,
  0x20,
  0x20,
  0xFF,
  0x20
};

// Type 41 Onboard Devices Extended Information
STATIC ARM_TYPE41 mArmDefaultType41 = {
  {
    { // SMBIOS_STRUCTURE Hdr
      EFI_SMBIOS_TYPE_ONBOARD_DEVICES_EXTENDED_INFORMATION,
      sizeof (SMBIOS_TABLE_TYPE41),
      SMBIOS_HANDLE_PI_RESERVED,
    },
    1,
    0x83,  // OnBoardDeviceExtendedTypeVideo, Enabled
    1,
    4,
    2,
    0,
  },
  TYPE41_ADDITIONAL_STRINGS
};

STATIC CONST VOID *DefaultCommonTables[] =
{
  &mArmDefaultType8Vga,
  &mArmDefaultType8USBFront,
  &mArmDefaultType8USBRear,
  &mArmDefaultType8NetRJ45,
  &mArmDefaultType8NetOcp,
  &mArmDefaultType8Uart,
  &mArmDefaultType9Sk0RiserX32Slot1,
  &mArmDefaultType9Sk0RiserX32Slot2,
  &mArmDefaultType9Sk0RiserX32Slot3,
  &mArmDefaultType9Sk1RiserX24Slot1,
  &mArmDefaultType9Sk1RiserX24Slot2,
  &mArmDefaultType9Sk1RiserX24Slot3,
  &mArmDefaultType9Sk1RiserX8Slot1,
  &mArmDefaultType9Sk0OcpNic,
  &mArmDefaultType9Sk0NvmeM2Slot1,
  &mArmDefaultType9Sk0NvmeM2Slot2,
  &mArmDefaultType11,
  &mArmDefaultType24,
  &mArmDefaultType32,
  &mArmDefaultType38,
  &mArmDefaultType41,
  NULL
};

/**
   Install a whole table worth of structures

   @param  Smbios               SMBIOS protocol.
   @param  DefaultTables        A pointer to the default SMBIOS table structure.
**/
EFI_STATUS
InstallStructures (
  IN       EFI_SMBIOS_PROTOCOL *Smbios,
  IN CONST VOID                *DefaultTables[]
  )
{
  EFI_STATUS        Status = EFI_SUCCESS;
  EFI_SMBIOS_HANDLE SmbiosHandle;
  UINTN             TableIndex;

  ASSERT (Smbios != NULL);

  for (TableIndex = 0; DefaultTables[TableIndex] != NULL; TableIndex++) {
    SmbiosHandle = ((EFI_SMBIOS_TABLE_HEADER *)DefaultTables[TableIndex])->Handle;
    Status = Smbios->Add (
                       Smbios,
                       NULL,
                       &SmbiosHandle,
                       (EFI_SMBIOS_TABLE_HEADER *)DefaultTables[TableIndex]
                       );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: adding %d failed\n", __FUNCTION__, TableIndex));

      // stop adding rather than continuing
      return Status;
    }
  }

  return EFI_SUCCESS;
}

BOOLEAN
GetPinStatus (
  IN IO_EXPANDER_CONTROLLER *Controller,
  IN UINT8                  Pin
  )
{
  EFI_STATUS Status;
  UINT8      PinValue;

  Status = IOExpanderSetDir (
             Controller,
             Pin,
             CONFIG_IOEXPANDER_PIN_AS_INPUT
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to set IO pin direction\n", __FUNCTION__));
    return FALSE;
  }
  Status = IOExpanderGetPin (
             Controller,
             Pin,
             &PinValue
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get IO pin value\n", __FUNCTION__));
    return FALSE;
  }

  return (PinValue > 0) ? FALSE : TRUE;
}

VOID
UpdateSegmentGroupAltraMax (
  VOID
  )
{
  //
  // PCI Segment Groups defined in the system via the _SEG object in the ACPI name space.
  // Refer to AC02 ACPI tables to check the fields below.
  //
  // Socket 0 RCA2
  mArmDefaultType9Sk0OcpNic.Base.SegmentGroupNum = 1;
  // Socket 0 RCA3
  mArmDefaultType9Sk0RiserX32Slot2.Base.SegmentGroupNum = 0;
  // Socket 0 RCA7
  mArmDefaultType9Sk0RiserX32Slot1.Base.SegmentGroupNum = 5;
  mArmDefaultType9Sk0RiserX32Slot3.Base.SegmentGroupNum = 5;
  // Socket 0 RCA6
  mArmDefaultType9Sk0NvmeM2Slot1.Base.SegmentGroupNum = 4;
  mArmDefaultType9Sk0NvmeM2Slot2.Base.SegmentGroupNum = 4;
  // Socket 1 RCA4
  mArmDefaultType9Sk1RiserX24Slot1.Base.SegmentGroupNum = 8;
  mArmDefaultType9Sk1RiserX8Slot1.Base.SegmentGroupNum  = 8;
  // Socket 1 RCA3
  mArmDefaultType9Sk1RiserX24Slot2.Base.SegmentGroupNum = 7;
  mArmDefaultType9Sk1RiserX24Slot3.Base.SegmentGroupNum = 7;
}

VOID
UpdateSmbiosType9 (
  VOID
  )
{
  IO_EXPANDER_CONTROLLER Controller;

  //
  // Set IOExpander controller for Riser x32
  //
  Controller.ChipID     = IO_EXPANDER_TCA6424A;
  Controller.I2cBus     = S0_RISER_I2C_BUS;
  Controller.I2cAddress = S0_RISERX32_I2C_ADDRESS;

  // Slot 1
  if (GetPinStatus (&Controller, S0_RISERX32_SLOT1_PRESENT_PIN1)
    || GetPinStatus (&Controller, S0_RISERX32_SLOT1_PRESENT_PIN2))
  {
    mArmDefaultType9Sk0RiserX32Slot1.Base.CurrentUsage = SlotUsageInUse;
  } else {
    mArmDefaultType9Sk0RiserX32Slot1.Base.CurrentUsage = SlotUsageAvailable;
  }
  // Slot 2
  if (GetPinStatus (&Controller, S0_RISERX32_SLOT2_PRESENT_PIN1)
    || GetPinStatus (&Controller, S0_RISERX32_SLOT2_PRESENT_PIN2)
    || GetPinStatus (&Controller, S0_RISERX32_SLOT2_PRESENT_PIN3))
  {
    mArmDefaultType9Sk0RiserX32Slot2.Base.CurrentUsage = SlotUsageInUse;
  } else {
    mArmDefaultType9Sk0RiserX32Slot2.Base.CurrentUsage = SlotUsageAvailable;
  }
  // Slot 3
  if (GetPinStatus (&Controller, S0_RISERX32_SLOT3_PRESENT_PIN1)
    || GetPinStatus (&Controller, S0_RISERX32_SLOT3_PRESENT_PIN2))
  {
    mArmDefaultType9Sk0RiserX32Slot3.Base.CurrentUsage = SlotUsageInUse;
  } else {
    mArmDefaultType9Sk0RiserX32Slot3.Base.CurrentUsage = SlotUsageAvailable;
  }

  //
  // Set IOExpander controller for OCP NIC
  //
  Controller.ChipID = IO_EXPANDER_TCA9534;
  Controller.I2cBus = S0_OCP_I2C_BUS;
  Controller.I2cAddress = S0_OCP_I2C_ADDRESS;

  if (GetPinStatus (&Controller, S0_OCP_SLOT_PRESENT_PIN1)
    || GetPinStatus (&Controller, S0_OCP_SLOT_PRESENT_PIN2)
    || GetPinStatus (&Controller, S0_OCP_SLOT_PRESENT_PIN3)
    || GetPinStatus (&Controller, S0_OCP_SLOT_PRESENT_PIN4))
  {
    mArmDefaultType9Sk0OcpNic.Base.CurrentUsage = SlotUsageInUse;
  } else {
    mArmDefaultType9Sk0OcpNic.Base.CurrentUsage = SlotUsageAvailable;
  }

  if (IsSlaveSocketActive ()) {
    //
    // Set IOExpander controller for Riser x24
    //
    Controller.ChipID = IO_EXPANDER_TCA6424A;
    Controller.I2cBus = S1_RISER_I2C_BUS;
    Controller.I2cAddress = S1_RISERX24_I2C_ADDRESS;

    // Slot 1
    if (GetPinStatus (&Controller, S1_RISERX24_SLOT1_PRESENT_PIN1)
      || GetPinStatus (&Controller, S1_RISERX24_SLOT1_PRESENT_PIN2))
    {
      mArmDefaultType9Sk1RiserX24Slot1.Base.CurrentUsage = SlotUsageInUse;
    } else {
      mArmDefaultType9Sk1RiserX24Slot1.Base.CurrentUsage = SlotUsageAvailable;
    }

    // Slot 2
    mArmDefaultType9Sk1RiserX24Slot2.Base.CurrentUsage =
      GetPinStatus (&Controller, S1_RISERX24_SLOT2_PRESENT_PIN) ? SlotUsageInUse : SlotUsageAvailable;

    // Slot 3
    mArmDefaultType9Sk1RiserX24Slot3.Base.CurrentUsage =
      GetPinStatus (&Controller, S1_RISERX24_SLOT3_PRESENT_PIN) ? SlotUsageInUse : SlotUsageAvailable;

    //
    // Set IOExpander controller for Riser x8
    //
    Controller.I2cAddress = S1_RISERX8_I2C_ADDRESS;
    if (GetPinStatus (&Controller, S1_RISERX8_SLOT1_PRESENT_PIN1)
      || GetPinStatus (&Controller, S1_RISERX8_SLOT1_PRESENT_PIN2))
    {
      mArmDefaultType9Sk1RiserX8Slot1.Base.CurrentUsage = SlotUsageInUse;
    } else {
      mArmDefaultType9Sk1RiserX8Slot1.Base.CurrentUsage = SlotUsageAvailable;
    }
  }

  //
  // According to Mt.Jade schematic for Altra Max, PCIe block diagram,
  // system slots connect to different Root Complex when we compare with
  // Mt.Jade schematic for Altra, so the segment group is changed.
  //
  if ( !IsAc01Processor ()) {
    UpdateSegmentGroupAltraMax ();
  }
}

/**
  Fills necessary information of SLC in SMBIOS Type 7.

  @param[out]   Type7Record            The Type 7 structure to allocate and initialize.

  @retval       EFI_SUCCESS            The Type 7 structure was successfully
                                       allocated and the strings initialized.
                EFI_OUT_OF_RESOURCES   Could not allocate memory needed.
**/
EFI_STATUS
ConfigSLCArchitectureInformation (
  OUT SMBIOS_TABLE_TYPE7  **Type7Record
  )
{
  CHAR8 *CacheSocketStr;
  CHAR8 *OptionalStart;
  UINTN CacheSocketStrLen;
  UINTN TableSize;

  CacheSocketStr = AllocateZeroPool (SMBIOS_STRING_MAX_LENGTH);
  if (CacheSocketStr == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  CacheSocketStrLen = AsciiSPrint (
                        CacheSocketStr,
                        SMBIOS_STRING_MAX_LENGTH - 1,
                        "L%d Cache (SLC)",
                        MAX_CACHE_LEVEL + 1
                        );

  TableSize   = sizeof (SMBIOS_TABLE_TYPE7) + CacheSocketStrLen + 1 + 1;
  *Type7Record = AllocateZeroPool (TableSize);
  if (*Type7Record == NULL) {
    FreePool (CacheSocketStr);
    return EFI_OUT_OF_RESOURCES;
  }

  (*Type7Record)->Hdr.Type   = EFI_SMBIOS_TYPE_CACHE_INFORMATION;
  (*Type7Record)->Hdr.Length = sizeof (SMBIOS_TABLE_TYPE7);
  (*Type7Record)->Hdr.Handle = SMBIOS_HANDLE_PI_RESERVED;

  /* Cache Configuration */
  (*Type7Record)->CacheConfiguration  = (UINT16)(CacheModeWriteBack << CACHE_OPERATION_MODE_SHIFT);
  (*Type7Record)->CacheConfiguration |= (UINT16)(CacheLocationInternal << CACHE_LOCATION_SHIFT);
  (*Type7Record)->CacheConfiguration |= (UINT16)(1 << CACHE_ENABLED_SHIFT);
  (*Type7Record)->CacheConfiguration |= (UINT16)(MAX_CACHE_LEVEL);

  /* Cache Size */
  if (IsAc01Processor ()) {
    //
    // Altra's SLC size is 32MB
    //
    (*Type7Record)->MaximumCacheSize = SLC_SIZE (32);
    (*Type7Record)->MaximumCacheSize2 = SLC_SIZE_2 (32);
  } else {
    //
    // Altra Max's SLC size is 16MB
    //
    (*Type7Record)->MaximumCacheSize = SLC_SIZE (16);
    (*Type7Record)->MaximumCacheSize2 = SLC_SIZE_2 (16);
  }
  (*Type7Record)->InstalledSize = (*Type7Record)->MaximumCacheSize;
  (*Type7Record)->InstalledSize2 = (*Type7Record)->MaximumCacheSize2;

  /* Other information of SLC */
  (*Type7Record)->SocketDesignation             = 1;
  (*Type7Record)->SupportedSRAMType.Synchronous = 1;
  (*Type7Record)->CurrentSRAMType.Synchronous   = 1;
  (*Type7Record)->CacheSpeed                    = 0;
  (*Type7Record)->SystemCacheType               = CacheTypeUnified;
  (*Type7Record)->Associativity                 = CacheAssociativity16Way;
  (*Type7Record)->ErrorCorrectionType           = CacheErrorSingleBit;

  OptionalStart = (CHAR8 *)(*Type7Record + 1);
  CopyMem (OptionalStart, CacheSocketStr, CacheSocketStrLen + 1);
  FreePool (CacheSocketStr);

  return EFI_SUCCESS;
}

/**
  Checks whether SLC cache is should be displayed or not.

  @return   TRUE    Should be displayed.
            FALSE   Should not be displayed.
**/
BOOLEAN
CheckSlcCache (
  VOID
  )
{
  EFI_STATUS        Status;
  UINT32            NumaMode;
  UINTN             CpuConfigDataSize;
  CPU_VARSTORE_DATA CpuConfigData;

  CpuConfigDataSize = sizeof (CPU_VARSTORE_DATA);

  Status = gRT->GetVariable (
                  CPU_CONFIG_VARIABLE_NAME,
                  &gCpuConfigFormSetGuid,
                  NULL,
                  &CpuConfigDataSize,
                  &CpuConfigData
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Can not get CPU configuration information - %r\n", __FUNCTION__, Status));

    Status = NVParamGet (
               NV_SI_SUBNUMA_MODE,
               NV_PERM_ATF | NV_PERM_BIOS | NV_PERM_MANU | NV_PERM_BMC,
               &NumaMode
               );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Can not get SubNUMA mode - %r\n", __FUNCTION__, Status));
      NumaMode = SUBNUMA_MODE_MONOLITHIC;
    }

    if (!IsSlaveSocketActive () && (NumaMode == SUBNUMA_MODE_MONOLITHIC)) {
      return TRUE;
    }
  } else if (CpuConfigData.CpuSlcAsL3 == CPU_SLC_AS_L3_ENABLE) {
    return TRUE;
  }

  return FALSE;
}

/**
  Installs Type 7 structure for SLC.

  @param[in]   Smbios        SMBIOS protocol to add type structure.

  @retval      EFI_SUCCESS   Installed Type7 structure of SLC  sucessfully.
               Other value   Failed to install Type7 structure of SLC.
**/
EFI_STATUS
InstallType7SlcStructure (
  IN EFI_SMBIOS_PROTOCOL *Smbios
  )
{
  EFI_STATUS         Status;
  SMBIOS_TABLE_TYPE7 *Type7Record;
  EFI_SMBIOS_HANDLE  SmbiosHandle;

  if (CheckSlcCache ()) {
    Status = ConfigSLCArchitectureInformation (&Type7Record);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    SmbiosHandle = SMBIOS_HANDLE_PI_RESERVED;
    Status = Smbios->Add (
                       Smbios,
                       NULL,
                       &SmbiosHandle,
                       (EFI_SMBIOS_TABLE_HEADER *)Type7Record
                       );

    FreePool (Type7Record);
  }

  return Status;
}

/**
   Install all structures from the DefaultTables structure

   @param  Smbios               SMBIOS protocol

**/
EFI_STATUS
InstallAllStructures (
  IN EFI_SMBIOS_PROTOCOL *Smbios
  )
{
  EFI_STATUS Status = EFI_SUCCESS;

  ASSERT (Smbios != NULL);

  // Update SMBIOS Type 9 Table
  UpdateSmbiosType9 ();

  // Install Type 7 for System Level Cache (SLC)
  Status = InstallType7SlcStructure (Smbios);
  ASSERT_EFI_ERROR (Status);

  // Install Tables
  Status = InstallStructures (Smbios, DefaultCommonTables);
  ASSERT_EFI_ERROR (Status);

  return Status;
}

/**
   Installs SMBIOS information for ARM platforms

   @param ImageHandle     Module's image handle
   @param SystemTable     Pointer of EFI_SYSTEM_TABLE

   @retval EFI_SUCCESS    Smbios data successfully installed
   @retval Other          Smbios data was not installed

**/
EFI_STATUS
EFIAPI
SmbiosPlatformDxeEntry (
  IN EFI_HANDLE       ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
  )
{
  EFI_STATUS          Status;
  EFI_SMBIOS_PROTOCOL *Smbios;

  //
  // Find the SMBIOS protocol
  //
  Status = gBS->LocateProtocol (
                  &gEfiSmbiosProtocolGuid,
                  NULL,
                  (VOID **)&Smbios
                  );

  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = InstallAllStructures (Smbios);
  DEBUG ((DEBUG_ERROR, "SmbiosPlatform install - %r\n", Status));

  return Status;
}
