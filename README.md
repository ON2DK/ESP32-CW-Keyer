# ESP32 CW Keyer

Open-source ESP32 CW keyer and Morse trainer designed by **ON2DK**.

This is the extended version of the stand-alone Morse Trainer project. It uses a **38-pin ESP32 DevKit with ESP32-WROOM-32D** and adds radio-interface functions for CAT, CW KEY and PTT together with four memory/program buttons.

## Main features

- ESP32-WROOM-32D DevKit, 38-pin version
- ST7789 240x320 SPI TFT support
- Paddle input
- Straight-key input
- Rotary encoder with push switch for menu/settings
- Four memory/program buttons
- Audio/sidetone section with LM386 and buzzer
- CAT interface
- CW KEY output
- PTT output
- Separate 12 V DC input
- TSR 1-2450 12 V to 5 V DC/DC converter
- POWER LED on the switched 12 V supply
- USB on the ESP32 DevKit for programming
- Predominantly through-hole construction

## PCB status

The hardware design has been completed and checked in KiCad. The final V2 board is a 2-layer FR-4 PCB with four M3 mounting holes. Final KiCad checks reported:

- 0 DRC violations
- 0 unconnected items
- 0 schematic/PCB parity errors

The final PCB outline is approximately **227.75 x 132.86 mm**.

The buzzer position was adjusted in the final layout to make its connector easier to access.

## Repository structure

- `KiCad/` - editable KiCad source archive and project notes
- `Gerber/` - production Gerber and drill archive
- `Firmware/` - Arduino IDE firmware and firmware notes
- `Enclosure/` - 3D-printable enclosure files and OpenSCAD sources
- `Documentation/` - project logbook, BOM and build notes
- `Images/` - PCB, schematic and 3D-view images

## Production files

The current source packages are available here:

- `KiCad/ESP32_CW_KEYER_V2_KiCad_Source.zip`
- `Gerber/ESP32_CW_KEYER_V2_Gerber.zip`

Before PCB production, open the Gerbers in a Gerber viewer and verify copper, solder mask, silkscreen, Edge.Cuts and drill files.

## Power and programming

Normal operation uses the separate **12 V DC input**. The ESP32 DevKit USB connector is intended for programming. During USB programming, the external 12 V supply should be switched off.

## Radio interface caution

Before connecting a transceiver, first verify the power rails and test CAT, KEY and PTT separately.

The board uses **2N7000 MOSFETs** in the radio-interface section. Check the exact transistor datasheet before soldering. The PCB design expects the electrical pin mapping **1 = Source, 2 = Gate, 3 = Drain**; not every manufacturer/package orientation is necessarily identical.

## Important build note

Always compare the physical pinout and dimensions of purchased components with the KiCad footprints before soldering. Pay particular attention to polarized parts, connector orientation, transistor pinouts and the ESP32 antenna keep-out area.

## Project status

Hardware: complete / production files prepared.

Firmware, enclosure details and documentation may continue to evolve in later revisions.
