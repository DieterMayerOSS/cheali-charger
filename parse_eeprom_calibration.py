#!/usr/bin/env python3
"""
Reads eeprom_backup.bin (dumped with avrdude) and prints calibration values
in the format ready to paste into defaultCalibration.cpp.
Create backup with command :  
  avrdude -patmega32 -cusbasp -Ueeprom:r:eeprom_backup.bin:r
Usage:
  python3 parse_eeprom_calibration.py [eeprom_backup.bin]
"""

import struct
import sys

EEPROM_FILE = sys.argv[1] if len(sys.argv) > 1 else "eeprom_backup.bin"

# Header layout (bytes):
#  4  magicString   "chli"
#  2  architecture
#  2  architectureInfo
#  2  calibrationVersion
#  2  programDataVersion
#  2  settingVersion
# = 14 bytes before calibration data

HEADER_SIZE = 14

# Physical inputs in order (for MAX_BALANCE_CELLS == 6, atmega32 target)
INPUTS = [
    "Vout_plus_pin",
    "Vout_minus_pin",
    "Ismps",
    "Idischarge",
    "VoutMux",
    "Tintern",
    "Vin",
    "Textern",
    "Vb0_pin",
    "Vb1_pin",
    "Vb2_pin",
    "Vb3_pin",
    "Vb4_pin",
    "Vb5_pin",
    "Vb6_pin",
    "IsmpsSet",
    "IdischargeSet",
]

# How to display the 'y' value for each input
# Format: (unit_name, scale_factor, format_string)
# y values are stored as: mV*1000 for volts, mA*1000 for amps, mC*1000 for temp
# ANALOG_VOLT(x)   = x * 1000  (stored as uint16 in mV, so 4200 = 4.200V)
# ANALOG_AMP(x)    = x * 1000  (stored as uint16 in mA, so 1000 = 1.000A)
# ANALOG_CELCIUS(x)= x * 100
UNIT_MAP = {
    "Vout_plus_pin":  ("ANALOG_VOLT",    1000.0, "%.3f"),
    "Vout_minus_pin": ("ANALOG_VOLT",    1000.0, "%.3f"),
    "Ismps":          ("ANALOG_AMP",     1000.0, "%.3f"),
    "Idischarge":     ("ANALOG_AMP",     1000.0, "%.3f"),
    "VoutMux":        ("ANALOG_VOLT",    1000.0, "%.3f"),
    "Tintern":        ("ANALOG_CELCIUS", 100.0,  "%.3f"),
    "Vin":            ("ANALOG_VOLT",    1000.0, "%.3f"),
    "Textern":        (None,             1.0,    "%d"),   # raw NTC table index
    "Vb0_pin":        ("ANALOG_VOLT",    1000.0, "%.3f"),
    "Vb1_pin":        ("ANALOG_VOLT",    1000.0, "%.3f"),
    "Vb2_pin":        ("ANALOG_VOLT",    1000.0, "%.3f"),
    "Vb3_pin":        ("ANALOG_VOLT",    1000.0, "%.3f"),
    "Vb4_pin":        ("ANALOG_VOLT",    1000.0, "%.3f"),
    "Vb5_pin":        ("ANALOG_VOLT",    1000.0, "%.3f"),
    "Vb6_pin":        ("ANALOG_VOLT",    1000.0, "%.3f"),
    "IsmpsSet":       ("ANALOG_AMP",     1000.0, "%.3f"),
    "IdischargeSet":  ("ANALOG_AMP",     1000.0, "%.3f"),
}

def format_point(name, x, y):
    unit, scale, fmt = UNIT_MAP[name]
    if unit is None:
        # Textern: raw values, just print as-is
        return f"{{{x}, {y}}}"
    val = y / scale
    return f"{{{x}, {unit}({fmt % val})}}"

def main():
    try:
        with open(EEPROM_FILE, "rb") as f:
            data = f.read()
    except FileNotFoundError:
        print(f"ERROR: File '{EEPROM_FILE}' not found.")
        print("Run first:  avrdude -patmega32 -cusbasp -Ueeprom:r:eeprom_backup.bin:r")
        sys.exit(1)

    magic = data[0:4]
    if magic != b"chli":
        print(f"WARNING: Magic string is {magic!r}, expected b'chli'.")
        print("         The EEPROM may be empty or from a different firmware.")
        print()

    print(f"EEPROM size read: {len(data)} bytes")
    print(f"Magic: {magic}")
    print()

    # Parse calibration: each entry = 2x CalibrationPoint = 2x (uint16, uint16) = 8 bytes
    offset = HEADER_SIZE
    calibration = []
    for name in INPUTS:
        p0_x, p0_y, p1_x, p1_y = struct.unpack_from("<HHHH", data, offset)
        calibration.append((name, p0_x, p0_y, p1_x, p1_y))
        offset += 8

    print("// === Paste this into defaultCalibration.cpp ===")
    print()
    print("const AnalogInputs::DefaultValues AnalogInputs::inputsP_[] PROGMEM = {")
    print()

    for name, p0_x, p0_y, p1_x, p1_y in calibration:
        p0 = format_point(name, p0_x, p0_y)
        p1 = format_point(name, p1_x, p1_y)
        line = f"  {{{p0}, {p1}}},"
        # pad to align comments
        pad = max(0, 70 - len(line))
        print(f"{line}{' ' * pad}//{name}")

    print()
    print("#if MAX_BALANCE_CELLS > 6")
    print("  // Add Vb7_pin and Vb8_pin entries here if needed")
    print("#endif")
    print()
    print("};")
    print()
    print("namespace {")
    print("  void assert() {")
    print("    STATIC_ASSERT(sizeOfArray(AnalogInputs::inputsP_) == AnalogInputs::PHYSICAL_INPUTS);")
    print("  }")
    print("}")


if __name__ == "__main__":
    main()
