# Gerber production files

The current V2 production package is:

- `ESP32_CW_KEYER_V2_Gerber.zip`

The package is intended for a 2-layer FR-4 PCB and contains the production Gerber and drill files generated from the final KiCad project.

Before ordering boards, inspect the archive in a Gerber viewer and verify at minimum:

- F.Cu and B.Cu
- F.Mask and B.Mask
- F.Silkscreen and B.Silkscreen
- Edge.Cuts
- plated and non-plated drill files

If the KiCad project is changed, generate a new Gerber package only after DRC and schematic/PCB parity checks are clean again.
