#!/usr/bin/env python3
"""Compare the desktop decoder with isolated Sony ARM64 helpers from one pinned IPA.

Requires the optional audit dependency `unicorn` (not needed by the desktop app).
Only leaf helpers run in emulated memory: no iOS app, Bluetooth, files or network
are available to the emulated code. The IPA and extracted code are never exported.
"""
import argparse
import hashlib
import json
import struct
import subprocess
import zipfile
from pathlib import Path

IPA_SHA256 = "163ec86d677bc1194c6028e016968be9415302b06da06284c6037f3d415f3297"
ROUTINES = {
    "unsigned_byte": (0x1012CA55C, 28),
    "valid_level": (0x1018BD208, 64),
    "single_level_index": (0x101416A38, 24),
    "single_charging_index": (0x101416A50, 24),
    "left_level_index": (0x101418224, 24),
    "left_charging_index": (0x10141823C, 24),
    "right_level_index": (0x101418254, 24),
    "right_charging_index": (0x10141826C, 24),
    "single_or_case_threshold_index": (0x10141F98C, 24),
}


def verify(ipa, checker=None):
    from unicorn import Uc, UC_ARCH_ARM64, UC_MODE_ARM
    from unicorn.arm64_const import UC_ARM64_REG_W0, UC_ARM64_REG_SP, UC_ARM64_REG_LR

    with ipa.open("rb") as stream:
        digest = hashlib.file_digest(stream, "sha256").hexdigest()
    if digest != IPA_SHA256:
        raise ValueError("IPA hash differs: these addresses apply only to Sony 13.2.0 / 510.9")
    with zipfile.ZipFile(ipa) as archive:
        binary = archive.read("Payload/MDRPlugin.app/MDRPlugin")
    if binary[:4] != b"\xcf\xfa\xed\xfe":
        raise ValueError("Expected 64-bit little-endian Mach-O")
    segments = []
    position = 32
    for _ in range(struct.unpack_from("<I", binary, 16)[0]):
        command, size = struct.unpack_from("<II", binary, position)
        if size < 8 or position + size > len(binary):
            raise ValueError("Invalid load command")
        if command == 0x19:
            vm, _, offset, file_size = struct.unpack_from("<QQQQ", binary, position + 24)
            segments.append((vm, offset, file_size))
        position += size

    def read(address, size):
        for vm, offset, file_size in segments:
            if vm <= address and address + size <= vm + file_size:
                return binary[offset + address - vm:offset + address - vm + size]
        raise ValueError("Address outside mapped file")

    def movz(address):
        instruction = struct.unpack("<I", read(address, 4))[0]
        if instruction & 0xffe00000 != 0x52800000:
            raise ValueError("Expected MOVZ W register")
        return (instruction >> 5) & 0xffff

    def cfstring(address):
        encoded = struct.unpack("<Q", read(address + 16, 8))[0]
        target = 0x100000000 + (encoded & ((1 << 36) - 1))
        length = struct.unpack("<Q", read(address + 24, 8))[0]
        return read(target, length).decode("ascii")

    charging = {}
    for name, literal, instruction, value in [
        ("NOT_CHARGING", 0x1032c6710, 0x10140e2f8, 0),
        ("CHARGING", 0x1032c6730, 0x10140e344, 1),
        ("UNKNOWN", 0x1032b2ef0, 0x10140e388, 2),
        ("CHARGED", 0x1032c86b0, 0x10140e3cc, 3),
    ]:
        if cfstring(literal) != name or movz(instruction) != value:
            raise ValueError("Unexpected charging enum initializer")
        charging[name] = value
    payload_sizes = [0] * 256
    for name, literal, instruction, value, size in [
        ("BATTERY", 0x1032c7450, 0x10140d138, 0, 4),
        ("LEFT_RIGHT_BATTERY", 0x1032c7470, 0x10140d184, 1, 6),
        ("CRADLE_BATTERY", 0x1032c7490, 0x10140d1c8, 2, 4),
        ("BATTERY_WITH_THRESHOLD", 0x1032c8590, 0x10140d360, 8, 5),
        ("LR_BATTERY_WITH_THRESHOLD", 0x1032c85b0, 0x10140d3a4, 9, 8),
        ("CRADLE_BATTERY_WITH_THRESHOLD", 0x1032c85d0, 0x10140d3e8, 10, 5),
    ]:
        if cfstring(literal) != name or movz(instruction) != value:
            raise ValueError("Unexpected battery type initializer")
        payload_sizes[value] = size
    if movz(0x10141ad04) != 6 or movz(0x10141addc) != 7:
        raise ValueError("Unexpected left/right threshold indices")

    battery_functions = [-1] * 256
    for name, literal, instruction, value, battery_type in [
        ("BATTERY_LEVEL_INDICATOR", 0x1032ce370, 0x1016d16f4, 0x20, 0),
        ("LEFT_RIGHT_BATTERY_LEVEL_INDICATOR", 0x1032ce390, 0x1016d1780, 0x21, 1),
        ("CRADLE_BATTERY_LEVEL_INDICATOR", 0x1032ce3b0, 0x1016d180c, 0x22, 2),
        ("BATTERY_LEVEL_WITH_THRESHOLD", 0x1032ce410, 0x1016d1b54, 0x28, 8),
        ("LR_BATTERY_LEVEL_WITH_THRESHOLD", 0x1032ce430, 0x1016d1be0, 0x29, 9),
        ("CRADLE_BATTERY_LEVEL_WITH_THRESHOLD", 0x1032ce450, 0x1016d1c6c, 0x2a, 10),
    ]:
        if cfstring(literal) != name or movz(instruction) != value:
            raise ValueError("Unexpected battery capability mapping")
        battery_functions[value] = battery_type
    if (cfstring(0x1032c3d70) != "CONNECT_GET_SUPPORT_FUNCTION" or movz(0x101494558) != 6 or
        cfstring(0x1032c3d90) != "CONNECT_RET_SUPPORT_FUNCTION" or movz(0x10149459c) != 7):
        raise ValueError("Unexpected capability command codes")

    fingerprints = {}
    functions = {}
    for name, (address, size) in ROUTINES.items():
        code = read(address, size)
        fingerprints[name] = {"address": hex(address), "size": size,
                              "sha256": hashlib.sha256(code).hexdigest()}
        machine = Uc(UC_ARCH_ARM64, UC_MODE_ARM)
        page = address & ~0xfff
        machine.mem_map(page, 0x1000)
        machine.mem_write(address, code)
        machine.mem_map(0x70000000, 0x2000)
        machine.mem_map(0x70010000, 0x1000)

        def execute(value=0, machine=machine, address=address):
            machine.reg_write(UC_ARM64_REG_SP, 0x70001000)
            machine.reg_write(UC_ARM64_REG_LR, 0x70010000)
            machine.reg_write(UC_ARM64_REG_W0, value & 0xffffffff)
            machine.emu_start(address, 0x70010000, count=200)
            from unicorn.arm64_const import UC_ARM64_REG_PC
            if machine.reg_read(UC_ARM64_REG_PC) != 0x70010000:
                raise ValueError("Helper did not return within instruction limit")
            return machine.reg_read(UC_ARM64_REG_W0)

        functions[name] = execute

    values = []
    for raw in range(256):
        sony_raw = functions["unsigned_byte"](raw)
        if sony_raw != raw:
            raise ValueError("Unexpected byte conversion")
        values.append(sony_raw if functions["valid_level"](sony_raw) else -1)
    for value in [-2147483648, -1, 0, 1, 99, 100, 101, 255, 2147483647]:
        if bool(functions["valid_level"](value)) != (0 <= value <= 100):
            raise ValueError("Unexpected Sony validity rule")
    indices = {name: function() for name, function in functions.items() if name.endswith("_index")}
    if indices != dict(single_level_index=2, single_charging_index=3, left_level_index=2,
                       left_charging_index=3, right_level_index=4, right_charging_index=5,
                       single_or_case_threshold_index=4):
        raise ValueError("Unexpected Sony field indices")
    matched = None
    if checker:
        client_values = json.loads(subprocess.check_output([str(checker.resolve()), "--dump-decoder"], text=True))
        if client_values != {"percentages": values, "payload_sizes": payload_sizes, "battery_functions": battery_functions}:
            raise ValueError("Desktop decoder differs from Sony helpers")
        matched = len(values)
    return {"ipa_sha256": digest, "version": "13.2.0", "build": "510.9",
            "method": "ARM64 leaf helpers executed in Unicorn; other findings from static disassembly",
            "routines": fingerprints, "field_indices": indices,
            "sony_valid_level_range": [0, 100], "zero_is_valid": True,
            "static_charging_codes": charging,
            "static_battery_functions": {hex(i): hex(n) for i, n in enumerate(battery_functions) if n >= 0},
            "static_battery_payload_sizes": {hex(i): n for i, n in enumerate(payload_sizes) if n},
            "static_left_right_threshold_indices": [6, 7],
            "byte_values_exercised": len(values), "desktop_values_matched": matched,
            "desktop_payload_sizes_matched": len(payload_sizes) if checker else None,
            "desktop_battery_function_mappings_matched": len(battery_functions) if checker else None,
            "limitations": "Does not execute the complete Sony app, validate device capabilities or compare a physical battery."}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("ipa", type=Path)
    parser.add_argument("--client-checker", type=Path, help="Built SonyBatteryTests executable")
    parser.add_argument("--output", type=Path, help="Save metadata and checksums only; never binary code")
    args = parser.parse_args()
    result = json.dumps(verify(args.ipa, args.client_checker), indent=2) + "\n"
    if args.output:
        args.output.write_text(result)
    print(result, end="")
