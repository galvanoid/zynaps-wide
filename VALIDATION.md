# Distribution candidate 0.7.1

Installation, first-launch preparation and gameplay on the owner's real Atari
have passed, including cached relaunch without repeating preparation. The
accepted 0.5.5 firmware and ZYPACE.PRG remain preserved in
the workspace's private validated release archive. Those older binaries contain
game resources and must not be published as downloads or previous versions.

Completed locally:

- The asset-free play executable boots and enters gameplay on a 68000 ST
  emulated at 8 MHz, with 1 MB and no Blitter.
- First-launch preparation on that ST produces a 447,776-byte private cache,
  byte-for-byte identical to the reference across 80 scenes and three tile banks.
- Cached subsequent startup enters gameplay with the same executable.
- 12,000 ordered full-screen compositions match the reconstruction, including
  clipping and pixel collision flags. Protocol tests cover all scenes, pages,
  scroll phases, reverse copy ordering and invalid requests.
- The importer checks original-file CRCs, handles missing/corrupt files, validates
  relocation before mutation and reproduces all twelve terrain projections.
- Host tests with address/undefined-behaviour sanitizers verify automatic PRG
  provisioning, cache validation before erase, recovery after incomplete flash
  programming, unchanged flash outside the reserved cache, no repeated writes
  for identical caches and preservation of the previous PRG on SD write failure.
- A source-only isolated Docker build, without mounting any original game files,
  builds both executables. The Atari PRG has the same SHA-256 as the main build.
  Firmware build timestamps may vary.
- UF2 writes end before 0x10092000. The private cache occupies only
  0x10092000..0x100fffff. Booster, ROM scratch and configuration remain outside
  both ranges. No cache bytes are emitted in the public UF2.
- The private resource audit compares 1,871 distinct nontrivial original/derived
  blocks against the public binaries, in addition to the explicit source and
  release file lists. Original disk images, resources and cache are excluded.

Completed on the real Atari:

- Booster installed 0.7.1 and verified its checksum. HTTP readback confirmed the
  automatically provisioned 81,254-byte executable matched the tested artifact.
- The missing-file message was verified with the diagnostic predecessor; the
  owner's 62 original files were then privately uploaded and read back exactly.
- The normal program launched from GEM and completed first-time preparation.
  The owner confirmed correct graphics, preserved smoothness and a visible boss.

The owner confirmed relaunch from GEM without repeating preparation. SD readback
confirmed the generated 447,776-byte cache matches the private reference exactly
(SHA-256 `8fb85c57ec2d52a181f95d41c11ce2f68c062a07522f72d7e023967d03e96768`).
Cartridge counters recorded 462 composed frames and 3,127 sprites with zero
protocol errors. Firmware tests verify an identical cache is not rewritten.

The isolated 0.7.1 build reproduces the Atari executable exactly. Its firmware
differs from the physically tested binary only in four build-date bytes. Publish
the original physically tested pair, identified by these SHA-256 checksums:

```text
834ccc5979a2caaed8f7a739d54ee2c8a018d61a896185d3dc9d9bf02ac8edc9  ZYWIDE.PRG
df3c7342ddf1bf77212887af1e0b799967809a4b08a2fb9fd92614f75260636b  ZYNAPS-SC.uf2
```
