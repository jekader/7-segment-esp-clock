#!/usr/bin/env python3
"""Package OTA and full-flash images for the 303WIFILC01 release."""

from hashlib import sha256
from pathlib import Path

PROJECT = Path(__file__).resolve().parent.parent
BUILD = PROJECT / '.pio' / 'build' / '303WIFILC01'
DIST = PROJECT / 'dist'
FLASH_SIZE = 0x100000
FS_OFFSET = 0xEB000
FS_SIZE = 0x10000
PREFIX = '7-segment-esp-clock'


def main() -> None:
    firmware = (BUILD / 'firmware.bin').read_bytes()
    filesystem = (BUILD / 'littlefs.bin').read_bytes()
    assert firmware, 'Firmware image is empty'
    assert len(filesystem) == FS_SIZE, (
        f'Expected {FS_SIZE} bytes of LittleFS, got {len(filesystem)}'
    )
    assert len(firmware) <= FS_OFFSET, 'Firmware overlaps the LittleFS offset'

    full = bytearray(b'\xff' * FLASH_SIZE)
    full[:len(firmware)] = firmware
    full[FS_OFFSET:FS_OFFSET + FS_SIZE] = filesystem

    DIST.mkdir(exist_ok=True)
    outputs = {
        f'{PREFIX}-ota.bin': firmware,
        f'{PREFIX}-full.bin': bytes(full),
    }
    for name, content in outputs.items():
        (DIST / name).write_bytes(content)
        print(f'{name}: {len(content)} bytes')

    checksums = ''.join(
        f'{sha256(content).hexdigest()}  {name}\n'
        for name, content in outputs.items()
    )
    (DIST / 'SHA256SUMS').write_text(checksums)


if __name__ == '__main__':
    main()
