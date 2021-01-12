/** @file

  Copyright (c) 2020, Ampere Computing LLC. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

DefinitionBlock("Dsdt.aml", "DSDT", 0x02, "Ampere", "Jade", 1) {
  //
  // Board Model
  Name(\BDMD, "Jade Board")
  Name(TPMF, 0x0)  // TPM presence
  Scope(\_SB) {

    Include ("CPU.asi")
    Include ("PMU.asi")

    //
    // Hardware Monitor
    Device(HM00) {
      Name(_HID, "APMC0D29")
      Name(_UID, "HWM0")
      Name(_DDN, "HWM0")
      Name(_CCA, ONE)
      Name(_STR, Unicode("Hardware Monitor Device"))
      Method(_STA, 0, NotSerialized) {
        return (0xF)
      }
      Name (_DSD, Package () {
        ToUUID("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
        Package() {
          Package() {"pcc-channel", 14}
        }
      })
    }

    //
    // Hardware Monitor
    Device(HM01) {
      Name(_HID, "APMC0D29")
      Name(_UID, "HWM1")
      Name(_DDN, "HWM1")
      Name(_CCA, ONE)
      Name(_STR, Unicode("Hardware Monitor Device"))
      Method(_STA, 0, NotSerialized) {
        return (0xF)
      }
      Name (_DSD, Package () {
        ToUUID("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
        Package() {
          Package() {"pcc-channel", 29}
        }
      })
    }

    //
    // Hardware Monitor
    Device(HM02) {
      Name(_HID, "AMPC0005")
      Name(_UID, "HWM2")
      Name(_DDN, "HWM2")
      Name(_CCA, ONE)
      Name(_STR, Unicode("AC01 SoC Hardware Monitor Device"))
      Method(_STA, 0, NotSerialized) {
        return (0xF)
      }
      Name(_CRS, ResourceTemplate() {
        QWordMemory (
          ResourceProducer,     // ResourceUsage
          PosDecode,            // Decode
          MinFixed,             // IsMinFixed
          MaxFixed,             // IsMaxFixed
          Cacheable,            // Cacheable
          ReadWrite,            // ReadAndWrite
          0x0000000000000000,   // AddressGranularity - GRA
          0x0000000088900000,   // AddressMinimum - MIN
          0x000000008891FFFF,   // AddressMaximum - MAX
          0x0000000000000000,   // AddressTranslation - TRA
          0x0000000000020000    // RangeLength - LEN
        )
      })
    }

    //
    // Hardware Monitor
    Device(HM03) {
      Name(_HID, "AMPC0005")
      Name(_UID, "HWM3")
      Name(_DDN, "HWM3")
      Name(_CCA, ONE)
      Name(_STR, Unicode("AC01 SoC Hardware Monitor Device"))
      Method(_STA, 0, NotSerialized) {
        return (0xF)
      }
      Name(_CRS, ResourceTemplate() {
        QWordMemory (
          ResourceProducer,     // ResourceUsage
          PosDecode,            // Decode
          MinFixed,             // IsMinFixed
          MaxFixed,             // IsMaxFixed
          Cacheable,            // Cacheable
          ReadWrite,            // ReadAndWrite
          0x0000000000000000,   // AddressGranularity - GRA
          0x0000000088920000,   // AddressMinimum - MIN
          0x000000008893FFFF,   // AddressMaximum - MAX
          0x0000000000000000,   // AddressTranslation - TRA
          0x0000000000020000    // RangeLength - LEN
        )
      })
    }

    //
    // DesignWare I2C on AHBC bus
    Device(I2C4) {
      Name(_HID, "APMC0D0F")
      Name(_UID, 4)
      Name(_STR, Unicode("eMAG2 I2C Device"))
      Method(_STA, 0, NotSerialized) {
        return (0x0f)
      }
      Name(_CCA, ONE)
      Name(_CRS, ResourceTemplate() {
        QWordMemory (
          ResourceProducer,     // ResourceUsage
          PosDecode,            // Decode
          MinFixed,             // IsMinFixed
          MaxFixed,             // IsMaxFixed
          NonCacheable,         // Cacheable
          ReadWrite,            // ReadAndWrite
          0x0000000000000000,   // AddressGranularity - GRA
          0x00001000026B0000,   // AddressMinimum - MIN
          0x00001000026BFFFF,   // AddressMaximum - MAX
          0x0000000000000000,   // AddressTranslation - TRA
          0x0000000000010000    // RangeLength - LEN
        )
        Interrupt(ResourceConsumer, Level, ActiveHigh, Exclusive) { 105 }
      })
      Device (IPI) {
        Name(_HID, "AMPC0004")
        Name(_CID, "IPI0001")
        Name(_STR, Unicode("IPMI_SSIF"))
        Name(_UID, 0)
        Name(_CCA, ONE)
        Method(_STA, 0, NotSerialized) {
          Return (0x0f)
        }
        Method(_IFT) {
          Return(0x04) // IPMI SSIF
        }
        Method(_ADR) {
          Return(0x10) // SSIF slave address
        }
        Method(_SRV) {
          Return(0x0200) // IPMI Specification Revision
        }
        Name(_CRS, ResourceTemplate ()
        {
          I2cSerialBus (0x0010, ControllerInitiated, 0x00061A80,
          AddressingMode7Bit, "\\_SB.I2C4",
          0x00, ResourceConsumer,,
          )
        })
      }
      Name(SSCN, Package() { 427, 499, 0 })
      Name(FMCN, Package() { 164, 319, 0 })
    }

    //
    // Report APEI Errors to GHES via SCI notification.
    // SCI notification requires one GED and one HED Device
    //     GED = Generic Event Device (ACPI0013)
    //     HED = Hardware Error Device (PNP0C33)
    //
    Device(GED0) {
        Name(_HID, "ACPI0013")
        Name(_UID, Zero)
        Method(_STA) {
          Return (0xF)
        }
        Name(_CRS, ResourceTemplate () {
            Interrupt(ResourceConsumer, Level, ActiveHigh, Exclusive) { 84 } // GHES
        })
        Method(_EVT, 1) {
          Switch (ToInteger(Arg0)) {
            Case (84) { // GHES interrupt
              Notify (HED0, 0x80)
            }
          }
        }
    }

    // Shutdown button using GED.
    Device(GED1) {
        Name(_HID, "ACPI0013")
        Name(_CID, "ACPI0013")
        Name(_UID, One)
        Method(_STA) {
          Return (0xF)
        }
        Name(_CRS, ResourceTemplate () {
          Interrupt(ResourceConsumer, Level, ActiveHigh, Exclusive) { 327 }
        })
        OperationRegion(PDDR, SystemMemory, 0x1000027B0004, 4)
        Field(PDDR, DWordAcc, NoLock, Preserve) {
          STDI, 8
        }

        OperationRegion(INTE, SystemMemory, 0x1000027B0030, 4)
        Field(INTE, DWordAcc, NoLock, Preserve) {
          STDE, 8
        }

        OperationRegion(INTT, SystemMemory, 0x1000027B0034, 4)
        Field(INTT, DWordAcc, NoLock, Preserve) {
          TYPE, 8
        }

        OperationRegion(INTP, SystemMemory, 0x1000027B0038, 4)
        Field(INTP, DWordAcc, NoLock, Preserve) {
          POLA, 8
        }

        OperationRegion(INTS, SystemMemory, 0x1000027B003c, 4)
        Field(INTS, DWordAcc, NoLock, Preserve) {
          STDS, 8
        }

        OperationRegion(INTC, SystemMemory, 0x1000027B0040, 4)
        Field(INTC, DWordAcc, NoLock, Preserve) {
          SINT, 8
        }

        OperationRegion(INTM, SystemMemory, 0x1000027B0044, 4)
        Field(INTM, DWordAcc, NoLock, Preserve) {
          MASK, 8
        }

        Method(_INI, 0, NotSerialized) {
          // Set level type, low active (shutdown)
          Store (0x00, TYPE)
          Store (0x00, POLA)
          // Set Input type (shutdown)
          Store (0x00, STDI)
          // Enable interrupt (shutdown)
          Store (0x80, STDE)
          // Unmask the interrupt.
          Store (0x00, MASK)
        }
        Method(_EVT, 1) {
          Switch (ToInteger(Arg0)) {
            Case (327) {
              if (And (STDS, 0x80)) {
                //Clear the interrupt.
                Store (0x80, SINT)
                // Notify OSPM the power button is pressed
                Notify (\_SB.PWRB, 0x80)
              }
            }
          }
        }
    }

    // Power button device description
    Device(PWRB) {
        Name(_HID, EISAID("PNP0C0C"))
        Name(_ADR, 0)
        Name(_UID, 0)
        Name(_CCA, ONE)
        Method(_STA, 0, Notserialized) {
            Return (0x0b)
        }
    }

    //
    // UART0 PL011
    Device(URT0) {
      Name(_HID, "ARMH0011")
      Name(_UID, 0)
      Name(_CCA, ONE)
      Method(_STA, 0, NotSerialized) {
        return (0x0f)
      }
      Name(_CRS, ResourceTemplate() {
        QWordMemory (
          ResourceProducer,     // ResourceUsage
          PosDecode,            // Decode
          MinFixed,             // IsMinFixed
          MaxFixed,             // IsMaxFixed
          NonCacheable,         // Cacheable
          ReadWrite,            // ReadAndWrite
          0x0000000000000000,   // AddressGranularity - GRA
          0x0000100002600000,   // AddressMinimum - MIN
          0x0000100002600FFF,   // AddressMaximum - MAX
          0x0000000000000000,   // AddressTranslation - TRA
          0x0000000000001000    // RangeLength - LEN
        )
        Interrupt(ResourceConsumer, Level, ActiveHigh, Exclusive) { 98 }
      })
    } // UART0

    //
    // UART2 PL011
    Device(URT2) {
      Name(_HID, "ARMH0011")
      Name(_UID, 1)
      Name(_CCA, ONE)
      Method(_STA, 0, NotSerialized) {
        return (0x0f)
      }
      Name(_CRS, ResourceTemplate() {
        QWordMemory (
          ResourceProducer,     // ResourceUsage
          PosDecode,            // Decode
          MinFixed,             // IsMinFixed
          MaxFixed,             // IsMaxFixed
          NonCacheable,         // Cacheable
          ReadWrite,            // ReadAndWrite
          0x0000000000000000,   // AddressGranularity - GRA
          0x0000100002620000,   // AddressMinimum - MIN
          0x0000100002620FFF,   // AddressMaximum - MAX
          0x0000000000000000,   // AddressTranslation - TRA
          0x0000000000001000    // RangeLength - LEN
        )
        Interrupt(ResourceConsumer, Level, ActiveHigh, Exclusive) { 100 }
      })
    } // UART1

    Device(HED0)
    {
        Name(_HID, EISAID("PNP0C33"))
        Name(_UID, Zero)
    }

    Device(NVDR) {
      Name(_HID, "ACPI0012")
      Method(_STA, 0, NotSerialized) {
        return (0xf)
      }
      Method (_DSM, 0x4, Serialized) {
        // Not support any functions for now
        Return (Buffer() {0})
      }
      Device (NVD1) {
        Name(_ADR, 1)
        Method(_STA, 0, NotSerialized) {
          return (0xf)
        }
      }
      Device (NVD2) {
        Name(_ADR, 2)
        Method(_STA, 0, NotSerialized) {
          return (0xf)
        }
      }
      Device (NVD3) {
        Name(_ADR, 3)
        Method(_STA, 0, NotSerialized) {
          return (0xf)
        }
      }
      Device (NVD4) {
        Name(_ADR, 4)
        Method(_STA, 0, NotSerialized) {
          return (0xf)
        }
      }
    }

    Device (TPM0) {
      Name (_HID, "MSFT0101")
      Name (_CID, "MSFT0101")
      Name (_UID, 0)
      Method (_CRS, 0x0, Serialized) {
        Name (RBUF, ResourceTemplate () {
          //QWORDMemory(ResourceConsumer, , MinFixed, MaxFixed, NonCacheable, ReadWrite, 0x00000000, 0x80300000, 0x80300FFF, 0x0, 0x1000,, ,)
          Memory32Fixed (ReadWrite, 0x88500000, 0x1000, PCRE)
        })
        Return (RBUF)
      }

      Method (_STR,0) {
        Return (Unicode ("TPM 2.0 Device"))
      }

      Method (_STA, 0) {
        if (TPMF) {
          Return (0x0f)  //Enable resources
        }
        Return (0x0)
      }
    }

    Include ("PCI-S0.Rca01.asi")
    Include ("PCI-S0.asi")
    Include ("PCI-S1.asi")
    Include ("PCI-PDRC.asi")
  }
} // DSDT
