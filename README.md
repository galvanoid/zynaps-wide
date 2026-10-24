# Zynaps Wide for SidecarTridge

Atari ST/STE accelerator with a 320 x 192 playfield and a compact, centred HUD.
Requires a SidecarTridge Multi-device and 1 MB of Atari RAM. The plain ST route
does not require a Blitter. This release keeps the validated VBL2 game cadence;
it does not claim 50 FPS.

**No original game files, disk image or prepared graphics are distributed.**
You must supply your own supported copy of Zynaps.

## Installation

1. Install the Zynaps Wide firmware through Booster. It creates the SD folder
   `/ZYNAPS` and writes the supplied, asset-free `ZYWIDE.PRG` there automatically.
2. Copy the 61 `.DAT`, `.MAP`, `.PIC` and `.PI1` files listed in `ORIGINAL-FILES.json`
   from your own game disk to that folder. Also copy `AUTO/ZYNAPS17.PRG` from
   the disk **as `/ZYNAPS/ZYNAPS17.PRG`**. Do not replace `ZYWIDE.PRG`.
3. Restart the Atari if it still shows Booster. After a three-second countdown,
   the firmware selects GEMDRIVE automatically. In GEM, open `C:\ZYWIDE.PRG`.
   GEMDRIVE maps the SD `/ZYNAPS` folder to drive C by default. Runner remains
   available for development and recovery.
4. The first launch checks your files and prepares `ZYWCACHE.BIN` locally.
   Progress dots indicate completed sections. Allow preparation to finish;
   do not reset or remove the SD while it is running.
5. The cartridge validates and stores the generated graphics in its reserved
   flash area. Subsequent launches reuse them; sprite composition reads flash
   during play rather than loading graphics from SD each frame. F10 returns
   to Runner/TOS.

This version supports the `ZYNAPS17.PRG` edition with CRC32 `238e7218` and the
exact resource checksums in `ORIGINAL-FILES.json`. Other editions need a separate
importer. A missing or mismatched resource is named on the Atari screen.
The original files and the generated private cache must not be included in
redistributed release archives.

An update retains the Wide application UUID and global settings. It sets only
this application's FOLDER and GEMDRIVE_FOLDER to `/ZYNAPS`; existing `/ZYWIDE`
folders are left on the SD. `ZYWIDE.OLD` preserves a replaced managed executable.

## Building

Run `python scripts/bootstrap.py` to fetch the pinned, asset-free reconstruction
and md-devops sources. The Atari build uses `stcmd` and the canonical
SidecarTridge toolkit container, extended with the freestanding m68k ELF compiler:

```powershell
docker build -f scripts/Dockerfile.atari -t logronoide/atarist-toolkit-docker-x86_64:zynaps .
.\scripts\build.ps1
.\scripts\build-firmware.ps1
```

Outputs: `dist/ZYWIDE.PRG` and `dist/ZYNAPS-SC.uf2`.
No original files are needed to compile either binary. Original files are needed
only for the private runtime tests and first launch on the Atari.

## Licences and source

The Atari executable incorporates the GPL-2.0 reconstruction by geogeo28:
https://github.com/geogeo28/atari_reverse at revision
`c1b35bfebb9f0466698f7cf211ecb04a22b0a8b1` (see `UPSTREAM-LICENSE`).
The independent RP2040 firmware incorporates GPL-3.0 md-devops:
https://github.com/sidecartridge/md-devops at revision
`1b5788d42b63f95ca340830f52da589869531f72` (see `FIRMWARE-LICENSE`).
Our additions are available under GPL-2.0-or-later; the two executable builds
follow their respective upstream licence requirements. SDK and toolchain
dependencies retain their own notices in the pinned dependency repositories.
The original Zynaps game data is not covered by these source-code licences.
