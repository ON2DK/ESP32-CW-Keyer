# ESP32 CW Keyer

Open-source ESP32 CW keyer designed by **ON2DK**.

The ESP32 CW Keyer is the expanded version of the Morse Trainer. It combines CW training and keying functions with memory/program buttons and radio-interface connections for CAT, KEY and PTT.

## Main features

- 38-pin ESP32 DevKit with ESP32-WROOM-32D
- ST7789 240x320 SPI TFT support
- Paddle and straight-key inputs
- Rotary encoder with push switch
- Audio/sidetone with LM386 and buzzer
- Four memory/program push buttons
- CAT interface
- KEY output
- PTT output
- Separate 12 V DC supply
- TSR 1-2450 12 V to 5 V DC/DC converter
- USB on the ESP32 DevKit for programming
- Predominantly through-hole construction

## PCB status

The V2 hardware revision has been completed in KiCad. The final design is a 2-layer PCB measuring approximately **227.75 x 132.86 mm**, with four M3 mounting holes. The final KiCad checks reported no DRC violations, no unconnected items and no schematic-parity problems before the production files were generated.

The buzzer position was adjusted in the final layout to improve access to its connector.

## Repository structure

- `KiCad/` - editable KiCad project files and project-local footprints
- `Gerber/` - production Gerber and drill files
- `Firmware/` - Arduino IDE firmware
- `Enclosure/` - 3D-printable enclosure files and OpenSCAD sources
- `Documentation/` - project logbook, BOM and build notes
- `Images/` - PCB, schematic and 3D-view images

## Power and programming

Normal operation uses the separate **12 V DC input**. The USB connector on the ESP32 DevKit is intended for programming. The external 12 V supply should be switched off while programming through USB.

## Radio connection warning

Before connecting a transceiver, verify the CAT, KEY and PTT interfaces independently and check the required signal levels and connector wiring for the specific radio. Prototype testing should be completed before connecting valuable radio equipment.

## Component pinout warning

Check purchased component pinouts against the KiCad footprints before soldering. This is particularly important for the **2N7000 MOSFETs** used in the interface section: the PCB mapping is designed around pins 1 = Source, 2 = Gate and 3 = Drain, but physical pin arrangements can vary by manufacturer/package.

## Project status

Hardware V2: complete / production files prepared.

Firmware, enclosure details and documentation may continue to evolve in later revisions.
