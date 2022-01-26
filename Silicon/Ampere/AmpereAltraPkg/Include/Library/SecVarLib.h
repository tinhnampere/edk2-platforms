/** @file

  Copyright (c) 2022, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef SECVAR_LIB_H_
#define SECVAR_LIB_H_

///
/// The FWU/FWA Trusted Certificate Variable name and vendor
/// guid for the authenticated variable.
///
#define AMPERE_FWU_CERT_NAME  L"dbu"
#define AMPERE_FWA_CERT_NAME  L"dbb"

#define AMPERE_CERT_VENDOR_GUID \
  {0x4796d3b0, 0x1bbb, 0x4680, {0xb4, 0x71, 0xa4, 0x9b, 0x49, 0xb2, 0x39, 0x0e}}

extern EFI_GUID  gAmpereCertVendorGuid;

/**
  Get secure variable via SMC

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
  );

/**
  Set/Update secure variable via SMC

  @param[in] mDigitalSigProtocol    Digital signature protocol
  @param[in] VariableName           Name of variable
  @param[in] VendorGuid             Guid of variable
  @param[in] Data                   Data pointer
  @param[in] DataSize               Size of Data

  @retval EFI_SUCCESS               Update operation successed
  @retval EFI_INVALID_PARAMETER     Invalid parameter
  @retval EFI_WRITE_PROTECTED       Write-protected variable
  @retval EFI_OUT_OF_RESOURCES      Not enough resource

**/
EFI_STATUS
SecVarSetSecureVariable (
  IN CHAR16   *VariableName,
  IN EFI_GUID *VendorGuid,
  IN UINT32   Attributes,
  IN VOID     *Data,
  IN UINTN    DataSize
  );

/**
  Enables secure variable authentication feature.

  @retval EFI_SUCCESS               Update operation success
  @retval EFI_DEVICE_ERROR          Not enough resource
 **/
EFI_STATUS
SecVarEnableKeyAuth (
  VOID
  );

#endif /* SECVAR_LIB_H_ */
