# Pinout

Pin assignments come from [`src/config.h`](src/config.h). The firmware addresses pins by
GPIO number, so the same build (`PICO_BOARD=pico`) runs on a Raspberry Pi Pico and on a
Waveshare RP2040-Zero.

## Wiring

| Pico pin (physical) | Function | Target signal | Notes |
|---|---|---|---|
| **GP0** (pin 1) | SWS (PIO, 12 mA, pull-up) | SWS | SWire data line. |
| **GND** (pin 3, 8, 13, …) | Ground | GND | |
| **GP4** (pin 6) | PWR (push-pull, 12 mA, high at boot) | Power | Powers the target; lets the tool power-cycle it for "activation". |

```
 Pico                         Target (TLSR82xx)
 ┌──────────────┐
 │ GP0  (pin 1) ├──────────────── SWS
 │ GND  (pin 3) ├──────────────── GND
 │ GP4  (pin 6) ├──────────────── Power
 └──────────────┘
```

## Using a Waveshare RP2040-Zero instead of a Pico

Only the header positions differ (seen from the top, USB-C connector up):

| Signal | RP2040-Zero pin | Location on the board |
|---|---|---|
| SWS | **GP0** | right edge, 1st pin from top |
| GND | **GND** | left edge, 2nd pin from top |
| PWR | **GP4** | right edge, 5th pin from top |

```
            USB-C
        ┌───────────┐
   5V   │●         ●│ GP0  ──── SWS
   GND  │●         ●│ GP1
   3V3  │●         ●│ GP2
   GP29 │●         ●│ GP3
   ...  │           │ GP4  ──── Power
```
GND goes to the target's GND.

The onboard WS2812 RGB LED on GP16 isn't used by the firmware and doesn't conflict. Check the
silkscreen before soldering, because the layout differs between clone boards.

## Notes

- **Activation.** SWS doesn't respond while the target is asleep or after its firmware has
  reassigned the SWS pin. TlsrPgm's `-t` (reset time) and `-a` (activation time) options
  switch **GP4 low** and back high, so the target is power-cycled and halted right after
  it boots, e.g. `TlsrPgm.py -p COMx -t 200 -a 2000 -s ...`.
- **Plain reads don't disturb the target.** Without `-t`/`-a`, TlsrPgm only queries the
  programmer and sets the SWire speed; GP4 stays high.
- **Supply.** GP4 is a GPIO at 12 mA drive strength, enough for a low-power target such as
  a coin-cell sensor, but its voltage sags under load spikes (e.g. radio TX).
