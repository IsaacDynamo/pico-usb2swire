# pico-usb2swire

A Telink TLSR82xx SWire programmer implementation for the Raspberry Pi Pico (RP2040).
Directly compatible with pvvx's [TLSRPGM](https://github.com/pvvx/TLSRPGM) as a USB2SWire reimplementation.

Uses PIO to drive SWS directly so ime way more consistent than UART shenanigans.

---

## Wiring

| Pico | Target | Notes |
|---|---|---|
| **GP0** | `SWS` | |
| **GP3** | `RST` | driven low or released open drain, never driven high. |
| **GP4** | `PWR` | 12 mA?, high at boot. Optional - the original had this but I didn't bother connecting it just did straight 3V3 so prob don't bother. |
| **GND** | `GND` | |

---

## Credit

All protocol and firmware design is [pvvx](https://github.com/pvvx)'s ([TLSRPGM](https://github.com/pvvx/TLSRPGM)).
