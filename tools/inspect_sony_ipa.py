#!/usr/bin/env python3
"""Inspect a user-supplied IPA locally without running or redistributing its code.

Class-name presence is structural evidence, not proof of command bytes or ranges.
"""
import argparse
import hashlib
import json
import plistlib
import struct
import zipfile
from pathlib import Path


def inspect(path):
    with zipfile.ZipFile(path) as archive:
        infos = [n for n in archive.namelist() if n.startswith("Payload/")
                 and n.count("/") == 2 and n.endswith(".app/Info.plist")]
        if len(infos) != 1:
            raise ValueError("Expected exactly one top-level application")
        info = plistlib.loads(archive.read(infos[0]))
        binary = archive.read(infos[0].rsplit("/", 1)[0] + "/" + info["CFBundleExecutable"])
    if binary[:4] != b"\xcf\xfa\xed\xfe":
        raise ValueError("Expected a little-endian 64-bit Mach-O")
    position = 32
    encryption_ids = []
    for _ in range(struct.unpack_from("<I", binary, 16)[0]):
        command, size = struct.unpack_from("<II", binary, position)
        if size < 8 or position + size > len(binary):
            raise ValueError("Invalid Mach-O load command")
        if command == 0x2c:
            encryption_ids.append(struct.unpack_from("<I", binary, position + 16)[0])
        position += size
    names = [
        "THMSGV1T1SetNcAsmParam", "THMSGV1T1RetNcAsmCapability",
        "THMSGV1T1SetEqEbbParam", "THMSGV1T1RetEqEbbCapability_EqCapability",
        "THMSGV2T1EqEbbRetCapabilityEq", "THMSGV2T1EqEbbSetParamEq",
        "HPCEqBandStepsStandard", "HPCEqBandSteps10band",
    ]
    with path.open("rb") as stream:
        digest = hashlib.file_digest(stream, "sha256").hexdigest()
    return {
        "sha256": digest,
        "bundle_id": info.get("CFBundleIdentifier"),
        "version": info.get("CFBundleShortVersionString"),
        "build": info.get("CFBundleVersion"),
        "accessory_protocols": info.get("UISupportedExternalAccessoryProtocols", []),
        "mach_o_cryptids": encryption_ids,
        "class_name_presence": {name: name.encode() + b"\0" in binary for name in names},
        "limitation": "Names do not establish opcodes, parameter ranges, device support, or hardware safety.",
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("ipa", type=Path)
    print(json.dumps(inspect(parser.parse_args().ipa), ensure_ascii=False, indent=2))
