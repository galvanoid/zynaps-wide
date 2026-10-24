# Catalog submission

The official creator contract is in
https://github.com/sidecartridge/sidecartridge-microfirmwares-store/blob/main/CREATORS.md

After physical validation, publish the clean source in `galvanoid/zynaps-wide`
and attach only `ZYNAPS-SC.uf2`, `ZYWIDE.PRG`, user instructions and checksums to
release `v0.7.1`. Do not attach private test folders or old embedded-data releases.

The repository's `apps.json` is the publisher-hosted app manifest. Its binary URL
must resolve to the published UF2 and match its MD5. Keep `previous_versions` empty
until an additional clean public version exists.

Submit the publisher origin to `origins/atari-st.json` in the official store:

```json
{
  "creator": {
    "id": "galvanoid",
    "name": "Galvanoid",
    "url": "https://github.com/galvanoid/zynaps-wide"
  },
  "official": false,
  "enabled": true,
  "url": "https://raw.githubusercontent.com/galvanoid/zynaps-wide/main/apps.json"
}
```

If that publisher already has an origin when submitting, update its hosted app
manifest instead of registering a duplicate creator. The catalog maintainers
merge the origin request; creating a GitHub release alone does not list the app.
