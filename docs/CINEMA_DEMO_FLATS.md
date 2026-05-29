# CINEMA_DEMO_FLATS.md — visible-in-game flats for the demo

**Author:** Scope (researcher) · **Date:** 2026-05-29 · **Game:** 2.3.1 (build 5314028, F-001)

Goal: 2–3 **scalar** flats whose modification is immediately observable in-game, with ready-to-drop YAML. Pipeline is proven end-to-end (E2E PASS @ a1c7d32); this supplies the *target*.

> **⚠ STATUS 2026-05-29 (post P1.12 probe + Q1/Q2 dig + 297-cand sweep):** every flat below was
> **REJECTED at runtime** (`flat not found`) — the P1.12 probe confirmed all 98
> ExtraFlats-derived candidates MISS the live flats map (F-022). ExtraFlats
> property names are not present on these records; the 193,354 live flats are
> dominated by runtime-generated inline/nested-record paths whose names are not
> in any offline string pool (F-020). A CRC32 brute-force against 6 known-live
> flat hashes (52M combos) found **no** name (Q2). A targeted 297-candidate
> sweep (11 confirmed-live records × 27 common property names: `damage`,
> `magazineCapacity`, `weaponVignetteIntensity`, `tags`, …) ALSO yielded
> **0/297 hits** — confirming that even canonical-looking `<record>.<property>`
> guesses miss. The flat name corpus is unreachable offline.
> **Do not use the picks below as-is.** Flat *write* IS proven (Schema wrote the
> unnamed flat `0xce8348b9`).
> **Next move to get NAMED cinema targets (in priority order):**
>   1. **In-process map walker** (P1.13 candidate) — walk +0x58 buckets, dump
>      first ~500 entries' TweakDBIDs + FlatValue {vft, value} payloads to log,
>      then hand-pick a Float/Int with a meaningful default and mutate it.
>      Doesn't need any name — proves cinema via blind mutation of a real flat.
>   2. **WolvenKit TweakDB string dump** — community-extracted name dictionary
>      (`tweakdbstr` from WolvenKit's Console projects, or `tweakdb_str.dat`
>      bundled with TweakXL on Windows). One-shot import gives us all real flat
>      names, end of guessing.
>   3. **RTTI dump at runtime** — enumerate per-record-type properties via the
>      game's reflection. Larger lift but gives a complete on-host map.
> The sections below are kept for provenance only.

---

## How this was discovered

1. **String-pool scan** of `r6/cache/tweakdb.bin` (`strings -8`): the on-disk pool stores **record names** (12,951 two-part IDs like `Items.Preset_Nova_Default`, `Attacks.MissileProjectile`) but **NOT flat property names** — flats are keyed by TweakDBID hash (see F-020). So a flat name = `<Record>.<property>`; the record is verifiable from the pool, the property from schema metadata.
2. **Property names + types** come from psiberx's `reference/windows-tweakxl/data/ExtraFlats.yaml`. Its top-level keys are **record TYPE class names** (confirmed via `MetadataImporter::ImportExtraFlats` → `IsRecordType()`), so each listed property is a real flat on **every** record of that type (e.g. `weaponVignetteIntensity: Float` on all `WeaponItem` records).
3. **Record existence** for every pick below was confirmed by exact-match `strings -8 tweakdb.bin | grep -xF`.

**Honest caveat:** the on-disk binary cannot confirm the record↔type binding (e.g. that `Attacks.MissileProjectile` is an `Attack_Projectile` record). That binding is inferred from naming + psiberx metadata. The cinema smoke test (`tools/test-cinema.sh`) is the runtime confirmation — a wrong binding logs `[applicator] reject op: flat not found`, not a crash.

---

## Recommended demo flats

### 1. Weapon screen vignette — HIGH confidence, reliable
- **Path:** `Items.Preset_Nova_Default.weaponVignetteIntensity`
- **Type:** `Float` · **Value:** `1.0` (default ~0)
- **Visible effect:** heavy dark vignette around the screen edges whenever the **Nova pistol** is equipped (first person).
- **Why high:** `Preset_*` are unambiguously `WeaponItem` records; `weaponVignetteIntensity` is a psiberx-documented `WeaponItem` Float.

```yaml
# _cinema_vignette.yaml  → r6/tweaks/
Items.Preset_Nova_Default.weaponVignetteIntensity:
  $type: Float
  $value: 1.0
Items.Preset_Nova_Default.weaponVignetteRadius:
  $type: Float
  $value: 1.0
```

### 2. Giant explosions — MEDIUM-HIGH confidence, most cinematic
- **Path:** `Attacks.MissileProjectile.explosionRadius` (and `explosionRange`)
- **Type:** `Float` · **Value:** `25.0` (default a few m)
- **Visible effect:** rocket-launcher / missile explosions become huge fireballs.
- **Why medium-high:** record confirmed; `explosionRadius`/`explosionRange` are psiberx-documented `Attack_Projectile` Floats; the name implies the type. Needs a rocket launcher to observe.

```yaml
# _cinema_boom.yaml  → r6/tweaks/
Attacks.MissileProjectile.explosionRadius:
  $type: Float
  $value: 25.0
Attacks.MissileProjectile.explosionRange:
  $type: Float
  $value: 25.0
```

### 3. Bigger smoke cloud — MEDIUM confidence, bonus
- **Path:** `Items.GrenadeSmokeRegular.smokeEffectRadius`
- **Type:** `Float` · **Value:** `30.0`
- **Visible effect:** smoke grenade produces a much larger cloud.
- **Why medium:** record confirmed; `smokeEffectRadius` is a psiberx-documented `Grenade` Float; record↔`Grenade`-type binding inferred. Needs a smoke grenade.

```yaml
# _cinema_smoke.yaml  → r6/tweaks/
Items.GrenadeSmokeRegular.smokeEffectRadius:
  $type: Float
  $value: 30.0
```

> **Fallback if all three reject** (`flat not found`): prove the pipeline with a guaranteed-apply record clone — `Items.CinemaClone: { $base: Items.Preset_Nova_Default }` always applies (creates a record), though it has no standalone visible effect. Then re-pick a flat from a record confirmed present, using a property from the matching `ExtraFlats.yaml` type block.

Each flat is in its **own file** on purpose: apply is **atomic per file** (one bad op rolls back that whole file), so isolating them means one bad pick can't sink the others.

---

## How to run the cinema demo

```bash
GAME_DIR="$HOME/Library/Application Support/Steam/steamapps/common/Cyberpunk 2077"
TWEAKS="$GAME_DIR/r6/tweaks"
mkdir -p "$TWEAKS"

# Drop the three demo files (copy the YAML blocks above):
#   $TWEAKS/_cinema_vignette.yaml
#   $TWEAKS/_cinema_boom.yaml
#   $TWEAKS/_cinema_smoke.yaml

# Launch with the loader injected (libtweakxl pulls libred4ext via its rpath dep):
DYLIB="/Users/lucas_1/Programming/cp2077-mac-mods/build/src/tweakxl-mac/libtweakxl.dylib"
DYLD_INSERT_LIBRARIES="$DYLIB" "$GAME_DIR/Cyberpunk2077.app/Contents/MacOS/Cyberpunk2077"

# In-game: load a save, then
#   • equip the Nova pistol            → screen vignette (flat 1)
#   • fire a rocket launcher / missile  → giant explosions (flat 2)
#   • throw a smoke grenade             → oversized smoke cloud (flat 3)

# Confirm application in the log:
grep '\[applicator\] mod applied:' /tmp/red4ext-mac.log
```

Automated smoke check (no manual play; just verifies a flat applies): `tools/test-cinema.sh`.
