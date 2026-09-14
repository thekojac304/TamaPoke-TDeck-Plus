#!/usr/bin/env python3
"""Build a hardware target without changing the web installer or touching NVS."""
import argparse
from pathlib import Path
import subprocess
import shutil

ROOT = Path(__file__).resolve().parent.parent
FQBN = ('esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi,'
        'PartitionScheme=app3M_fat9M_16MB')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('board', choices=('waveshare', 'tdeck-plus'))
    parser.add_argument('--cli', default='arduino-cli')
    parser.add_argument('--config-file')
    parser.add_argument('--touch-debug', action='store_true')
    parser.add_argument('--clean', action='store_true')
    args = parser.parse_args()
    # Arduino CLI copies every sketch subfolder, including local tool downloads.
    # Stage only firmware inputs so host tools/assets never enter that copy.
    sketch = ROOT / 'build' / ('input-' + args.board) / 'TamaPoke'
    if not sketch.resolve().is_relative_to((ROOT / 'build').resolve()):
        raise RuntimeError('Staging directory escaped the build folder')
    if sketch.exists():
        shutil.rmtree(sketch)
    sketch.mkdir(parents=True, exist_ok=True)
    for suffix in ('*.ino', '*.cpp', '*.h'):
        for source in ROOT.glob(suffix):
            shutil.copy2(source, sketch / source.name)
    shutil.copytree(ROOT / 'src', sketch / 'src', dirs_exist_ok=True)
    cmd = [args.cli, 'compile', '--fqbn', FQBN, '--warnings=all',
           '--build-path', str(ROOT / 'build' / ('cache-' + args.board)),
           '--output-dir', str(ROOT / 'build' / args.board)]
    if args.config_file:
        cmd += ['--config-file', str(Path(args.config_file).resolve())]
    flags = '-DTAMAPOKE_TDECK_PLUS=' + ('1' if args.board == 'tdeck-plus' else '0')
    if args.touch_debug:
        flags += ' -DTAMAPOKE_TOUCH_DEBUG=1'
    # Do not replace build.extra_flags: the core keeps USB/CDC defines there.
    cmd += ['--build-property', 'compiler.cpp.extra_flags=' + flags]
    if args.clean:
        cmd += ['--clean']
    subprocess.run(cmd + [str(sketch)], check=True, cwd=ROOT)
    for suffix in ('bin', 'bootloader.bin', 'partitions.bin', 'merged.bin'):
        artifact = ROOT / 'build' / args.board / ('TamaPoke.ino.' + suffix)
        if not artifact.is_file() or not artifact.stat().st_size:
            raise RuntimeError(f'Missing or empty firmware artifact: {artifact}')
    print(f'Firmware: {ROOT / "build" / args.board / "TamaPoke.ino.bin"}')


if __name__ == '__main__':
    main()
