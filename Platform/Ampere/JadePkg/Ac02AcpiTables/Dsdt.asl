/** @file

  Copyright (c) 2021, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

DefinitionBlock("Dsdt.aml", "DSDT", 0x02, "Ampere", "Jade", 1) {
  //
  // Board Model
  Name(\BDMD, "Altra Max Jade Board")
  Name(TPMF, 0)  // TPM presence
  Name(AERF, 0)  // PCIe AER Firmware-First
  Scope(\_SB) {

    Include ("CPU.asi")
    Include ("PMU.asi")
    Include ("CommonDevices.asi")

    Scope(\_SB.GED0) {
        Method(_EVT, 1, Serialized) {
          Switch (ToInteger(Arg0)) {
            Case (84) { // GHES interrupt
              Notify (HED0, 0x80)
            }

            Case (44) { // doorbell notification (Insertion/ejection)
                local0 = DOUT & 0x00FF0000
                if (local0 == 0x00010000) {
                    local0 = STA4 & 0xFFFFFFFF
                    if (local0) {
                        Store(local0, STA4) // clear interrupt
                    }
                    local0 = A120 & 0xFF000000
                    if (local0 == 0x01000000) {
                        Notify (\_SB.PCIA.P2P1, 1) // insertion action
                        Store(0x02000201, A120) // clear action value
                    }
                    local0 = A122 & 0xFF000000
                    if (local0 == 0x01000000) {
                        Notify (\_SB.PCIA.P2P3, 1) // insertion action
                        Store(0x02020201, A122) // clear action value
                    }
                }
                elseif (local0 == 0x00000000) {
                    local0 = STA4 & 0xFFFFFFFF
                    if (local0) {
                        Store(local0, STA4) // clear interrupt
                    }
                    local0 = A120 & 0xFF000000
                    if (local0 == 0x00000000) {
                        Notify (\_SB.PCIA.P2P1, 3) // ejection action
                        Store(0x02000201, A120) // clear action value
                    }
                    local0 = A122 & 0xFF000000
                    if (local0 == 0x00000000) {
                        Notify (\_SB.PCIA.P2P3, 3) // ejection action
                        Store(0x02020201, A122) // clear action value
                    }
                }
            }
          }
        }
    }

    Include ("PCI-S0.Rca01.asi")
    Include ("PCI-S0.asi")
    Include ("PCI-S1.asi")
    Include ("PCI-PDRC.asi")
  }
} // DSDT
