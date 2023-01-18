/** @file
  Configuration Manager Dxe

  Copyright (c) 2023, Ampere Computing. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <IndustryStandard/DebugPort2Table.h>
#include <IndustryStandard/IoRemappingTable.h>
#include <IndustryStandard/MemoryMappedConfigurationSpaceAccessTable.h>
#include <IndustryStandard/SerialPortConsoleRedirectionTable.h>
#include <Library/ArmLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/AcpiTable.h>
#include <Protocol/ConfigurationManagerProtocol.h>

#include "ConfigurationManager.h"

EDKII_PLATFORM_REPOSITORY_INFO  AmpereAltraRepositoryInfo = {
  // Configuration Manager information
  { CONFIGURATION_MANAGER_REVISION, CFG_MGR_OEM_ID },

  // ACPI table list
  {
    // // FADT Table
    // {
    //   EFI_ACPI_6_3_FIXED_ACPI_DESCRIPTION_TABLE_SIGNATURE,
    //   EFI_ACPI_6_3_FIXED_ACPI_DESCRIPTION_TABLE_REVISION,
    //   CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdFadt),
    //   (EFI_ACPI_DESCRIPTION_HEADER *)fadt_aml_code
    // },
    // // GTDT Table
    // {
    //   EFI_ACPI_6_3_GENERIC_TIMER_DESCRIPTION_TABLE_SIGNATURE,
    //   EFI_ACPI_6_3_GENERIC_TIMER_DESCRIPTION_TABLE_REVISION,
    //   CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdGtdt),
    //   (EFI_ACPI_DESCRIPTION_HEADER *)gtdt_aml_code
    // },
    // // SPCR Table
    // {
    //   EFI_ACPI_6_3_SERIAL_PORT_CONSOLE_REDIRECTION_TABLE_SIGNATURE,
    //   EFI_ACPI_SERIAL_PORT_CONSOLE_REDIRECTION_TABLE_REVISION,
    //   CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdSpcr),
    //   (EFI_ACPI_DESCRIPTION_HEADER *)spcr_aml_code
    // },
    // DSDT Table
    {
      EFI_ACPI_6_3_DIFFERENTIATED_SYSTEM_DESCRIPTION_TABLE_SIGNATURE,
      EFI_ACPI_6_3_DIFFERENTIATED_SYSTEM_DESCRIPTION_TABLE_REVISION,
      CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdDsdt),
      (EFI_ACPI_DESCRIPTION_HEADER *)dsdt_aml_code
    },
  },

  // Boot architecture information
  { EFI_ACPI_6_3_ARM_PSCI_COMPLIANT },              // BootArchFlags

  // Fixed feature flag information
  { EFI_ACPI_6_3_HEADLESS }                         // Fixed feature flags
};

/** A helper function for returning the Configuration Manager Objects.
  @param [in]       CmObjectId     The Configuration Manager Object ID.
  @param [in]       Object         Pointer to the Object(s).
  @param [in]       ObjectSize     Total size of the Object(s).
  @param [in]       ObjectCount    Number of Objects.
  @param [in, out]  CmObjectDesc   Pointer to the Configuration Manager Object
                                   descriptor describing the requested Object.
  @retval EFI_SUCCESS              Success.
**/
STATIC
EFI_STATUS
EFIAPI
HandleCmObject (
  IN  CONST CM_OBJECT_ID                CmObjectId,
  IN        VOID                        *Object,
  IN  CONST UINTN                       ObjectSize,
  IN  CONST UINTN                       ObjectCount,
  IN  OUT   CM_OBJ_DESCRIPTOR   *CONST  CmObjectDesc
  )
{
  CmObjectDesc->ObjectId = CmObjectId;
  CmObjectDesc->Size     = ObjectSize;
  CmObjectDesc->Data     = (VOID *)Object;
  CmObjectDesc->Count    = ObjectCount;
  DEBUG ((
    DEBUG_INFO,
    "INFO: CmObjectId = %x, Ptr = 0x%p, Size = %d, Count = %d\n",
    CmObjectId,
    CmObjectDesc->Data,
    CmObjectDesc->Size,
    CmObjectDesc->Count
    ));
  return EFI_SUCCESS;
}

/** Return an OEM namespace object.

  @param [in]        This        Pointer to the Configuration Manager Protocol.
  @param [in]        CmObjectId  The Configuration Manager Object ID.
  @param [in]        Token       An optional token identifying the object. If
                                 unused this must be CM_NULL_TOKEN.
  @param [in, out]   CmObject    Pointer to the Configuration Manager Object
                                 descriptor describing the requested Object.

  @retval EFI_SUCCESS            Success.
  @retval EFI_INVALID_PARAMETER  A parameter is invalid.
  @retval EFI_NOT_FOUND          The required object information is not found.
**/
EFI_STATUS
EFIAPI
GetOemNameSpaceObject (
  IN  CONST EDKII_CONFIGURATION_MANAGER_PROTOCOL  *CONST  This,
  IN  CONST CM_OBJECT_ID                                  CmObjectId,
  IN  CONST CM_OBJECT_TOKEN                               Token OPTIONAL,
  IN  OUT   CM_OBJ_DESCRIPTOR                     *CONST  CmObject
  )
{
  EFI_STATUS  Status;

  Status = EFI_SUCCESS;
  if ((This == NULL) || (CmObject == NULL)) {
    ASSERT (This != NULL);
    ASSERT (CmObject != NULL);
    return EFI_INVALID_PARAMETER;
  }

  switch (GET_CM_OBJECT_ID (CmObjectId)) {
    default:
    {
      Status = EFI_NOT_FOUND;
      DEBUG ((
        DEBUG_ERROR,
        "ERROR: Object 0x%x. Status = %r\n",
        CmObjectId,
        Status
        ));
      break;
    }
  }

  return Status;
}

/** Return an ARM namespace object.

  @param [in]        This        Pointer to the Configuration Manager Protocol.
  @param [in]        CmObjectId  The Configuration Manager Object ID.
  @param [in]        Token       An optional token identifying the object. If
                                 unused this must be CM_NULL_TOKEN.
  @param [in, out]   CmObject    Pointer to the Configuration Manager Object
                                 descriptor describing the requested Object.

  @retval EFI_SUCCESS            Success.
  @retval EFI_INVALID_PARAMETER  A parameter is invalid.
  @retval EFI_NOT_FOUND          The required object information is not found.
**/
EFI_STATUS
EFIAPI
GetArmNameSpaceObject (
  IN  CONST EDKII_CONFIGURATION_MANAGER_PROTOCOL  * CONST This,
  IN  CONST CM_OBJECT_ID                                  CmObjectId,
  IN  CONST CM_OBJECT_TOKEN                               Token OPTIONAL,
  IN  OUT   CM_OBJ_DESCRIPTOR                     * CONST CmObject
  )
{
  return EFI_NOT_FOUND;
}

/** Return a standard namespace object.

  @param [in]        This        Pointer to the Configuration Manager Protocol.
  @param [in]        CmObjectId  The Configuration Manager Object ID.
  @param [in]        Token       An optional token identifying the object. If
                                 unused this must be CM_NULL_TOKEN.
  @param [in, out]   CmObject    Pointer to the Configuration Manager Object
                                 descriptor describing the requested Object.

  @retval EFI_SUCCESS            Success.
  @retval EFI_INVALID_PARAMETER  A parameter is invalid.
  @retval EFI_NOT_FOUND          The required object information is not found.
**/
EFI_STATUS
EFIAPI
GetStandardNameSpaceObject (
  IN  CONST EDKII_CONFIGURATION_MANAGER_PROTOCOL  *CONST  This,
  IN  CONST CM_OBJECT_ID                                  CmObjectId,
  IN  CONST CM_OBJECT_TOKEN                               Token OPTIONAL,
  IN  OUT   CM_OBJ_DESCRIPTOR                     *CONST  CmObject
  )
{
  EFI_STATUS                      Status;
  EDKII_PLATFORM_REPOSITORY_INFO  *PlatformRepo;
  UINT32                          AcpiTableCount;

  if ((This == NULL) || (CmObject == NULL)) {
    ASSERT (This != NULL);
    ASSERT (CmObject != NULL);
    return EFI_INVALID_PARAMETER;
  }

  Status         = EFI_NOT_FOUND;
  PlatformRepo   = This->PlatRepoInfo;
  AcpiTableCount = ARRAY_SIZE (PlatformRepo->CmAcpiTableList);

  switch (GET_CM_OBJECT_ID (CmObjectId)) {
    case EStdObjCfgMgrInfo:
      Status = HandleCmObject (
                 CmObjectId,
                 &PlatformRepo->CmInfo,
                 sizeof (PlatformRepo->CmInfo),
                 1,
                 CmObject
                 );
      break;
    case EStdObjAcpiTableList:
      Status = HandleCmObject (
                 CmObjectId,
                 &PlatformRepo->CmAcpiTableList,
                 sizeof (PlatformRepo->CmAcpiTableList),
                 AcpiTableCount,
                 CmObject
                 );
      break;
    case EStdObjSmbiosTableList:
      // currently, we dont support SMBIOS
      Status = EFI_NOT_FOUND;
      DEBUG ((DEBUG_WARN, "DynamicTable: Ampere: SMBIOS: Dont supported\n"));
      break;
    default:
    {
      Status = EFI_NOT_FOUND;
      DEBUG ((
        DEBUG_ERROR,
        "ERROR: Object 0x%x. Status = %r\n",
        CmObjectId,
        Status
        ));
      break;
    }
  }

  return Status;
}

/** The GetObject function defines the interface implemented by the
    Configuration Manager Protocol for returning the Configuration
    Manager Objects.

  @param [in]        This        Pointer to the Configuration Manager Protocol.
  @param [in]        CmObjectId  The Configuration Manager Object ID.
  @param [in]        Token       An optional token identifying the object. If
                                 unused this must be CM_NULL_TOKEN.
  @param [in, out]   CmObject    Pointer to the Configuration Manager Object
                                 descriptor describing the requested Object.

  @retval EFI_SUCCESS            Success.
  @retval EFI_INVALID_PARAMETER  A parameter is invalid.
  @retval EFI_NOT_FOUND          The required object information is not found.
**/
EFI_STATUS
EFIAPI
AmpereAltraPlatformGetObject (
  IN  CONST EDKII_CONFIGURATION_MANAGER_PROTOCOL  *CONST  This,
  IN  CONST CM_OBJECT_ID                                  CmObjectId,
  IN  CONST CM_OBJECT_TOKEN                               Token OPTIONAL,
  IN  OUT   CM_OBJ_DESCRIPTOR                     *CONST  CmObject
  )
{
  EFI_STATUS  Status;

  if ((This == NULL) || (CmObject == NULL)) {
    ASSERT (This != NULL);
    ASSERT (CmObject != NULL);
    return EFI_INVALID_PARAMETER;
  }

  switch (GET_CM_NAMESPACE_ID (CmObjectId)) {
    case EObjNameSpaceStandard:
      Status = GetStandardNameSpaceObject (This, CmObjectId, Token, CmObject);
      break;
    case EObjNameSpaceArm:
      Status = GetArmNameSpaceObject (This, CmObjectId, Token, CmObject);
      break;
    case EObjNameSpaceOem:
      Status = GetOemNameSpaceObject (This, CmObjectId, Token, CmObject);
      break;
    default:
    {
      Status = EFI_INVALID_PARAMETER;
      DEBUG ((
        DEBUG_ERROR,
        "ERROR: Unknown Namespace Object = 0x%x. Status = %r\n",
        CmObjectId,
        Status
        ));
      break;
    }
  }

  return Status;
}

/** The SetObject function defines the interface implemented by the
    Configuration Manager Protocol for updating the Configuration
    Manager Objects.

  @param [in]        This        Pointer to the Configuration Manager Protocol.
  @param [in]        CmObjectId  The Configuration Manager Object ID.
  @param [in]        Token       An optional token identifying the object. If
                                 unused this must be CM_NULL_TOKEN.
  @param [in]        CmObject    Pointer to the Configuration Manager Object
                                 descriptor describing the Object.

  @retval EFI_UNSUPPORTED        This operation is not supported.
**/
EFI_STATUS
EFIAPI
AmpereAltraPlatformSetObject (
  IN  CONST EDKII_CONFIGURATION_MANAGER_PROTOCOL  *CONST  This,
  IN  CONST CM_OBJECT_ID                                  CmObjectId,
  IN  CONST CM_OBJECT_TOKEN                               Token OPTIONAL,
  IN        CM_OBJ_DESCRIPTOR                     *CONST  CmObject
  )
{
  return EFI_UNSUPPORTED;
}

/** Initialize the Platform Configuration Repository.
  @param [in]  PlatRepoInfo   Pointer to the Configuration Manager Protocol.
  @retval EFI_SUCCESS           Success
**/
STATIC
EFI_STATUS
EFIAPI
InitializePlatformRepository (
  IN  EDKII_PLATFORM_REPOSITORY_INFO  *CONST  PlatRepoInfo
  )
{
  return EFI_SUCCESS;
}

/** A structure describing the configuration manager protocol interface.
*/
STATIC
CONST
EDKII_CONFIGURATION_MANAGER_PROTOCOL  AmpereAltraPlatformConfigManagerProtocol = {
  CREATE_REVISION (1,           0),
  AmpereAltraPlatformGetObject,
  AmpereAltraPlatformSetObject,
  &AmpereAltraRepositoryInfo
};

/**
  Entrypoint of Configuration Manager Dxe.

  @param [in]  ImageHandle
  @param [in]  SystemTable

  @return EFI_SUCCESS
  @return EFI_LOAD_ERROR
  @return EFI_OUT_OF_RESOURCES
**/
EFI_STATUS
EFIAPI
ConfigurationManagerDxeInitialize (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;

  // Initialize the Platform Configuration Repository before installing the
  // Configuration Manager Protocol
  Status = InitializePlatformRepository (
             AmpereAltraPlatformConfigManagerProtocol.PlatRepoInfo
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "ERROR: Failed to initialize the Platform Configuration Repository." \
      " Status = %r\n",
      Status
      ));
  }

  Status = gBS->InstallProtocolInterface (
                  &ImageHandle,
                  &gEdkiiConfigurationManagerProtocolGuid,
                  EFI_NATIVE_INTERFACE,
                  (VOID *)&AmpereAltraPlatformConfigManagerProtocol
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "ERROR: Failed to get Install Configuration Manager Protocol." \
      " Status = %r\n",
      Status
      ));
    goto error_handler;
  }

error_handler:
  return Status;
}
