# Live-Edit Tool — User Guide (Phase 2)

A native macOS tool that **reads and writes the running game's memory** to change **stored counters**
(money/eddies, crafting components, attribute/perk points, XP) live, with one-command **revert**. It is
the productized form of the F-044 breakthrough.

**What it can change:** stored counters that exist in memory as an exact value.
**What it can't:** *derived/displayed* stats like RAM cap or max HP — those are computed from
base+modifiers and never stored as the number you see (FA-017/18/19). Use stored counters.

**Safety:** all edits are **RAM-only and revertable** — `revert` restores originals, and simply
**reloading your save** also restores everything. The tool never touches your save files. A
**version gate** disables writes if the game has been patched to a build we haven't validated (so stale
memory offsets can't corrupt it).

---

## Run it

```sh
/tmp/run_liveedit.sh
```

(or build + launch manually:)

```sh
cmake --build build --target tweakxl
GAME="$HOME/Library/Application Support/Steam/steamapps/common/Cyberpunk 2077/Cyberpunk2077.app/Contents/MacOS/Cyberpunk2077"
DYLIB="$PWD/build/src/tweakxl-mac/libtweakxl.dylib"
TWEAKXL_STAT_POKE=1 TWEAKXL_POKE_PROBE_CELLS=1 TWEAKXL_POKE_GO_FILE=/tmp/red4ext_go \
  DYLD_INSERT_LIBRARIES="$DYLIB" "$GAME"
```

Then **load your save and get in-world.** The tool waits for commands via the **go-file**
`/tmp/red4ext_go` and logs to `/tmp/red4ext-mac.log` (tag `[live-edit]`).

---

## Commands (write into the go-file)

| Command | Effect |
|---|---|
| `echo "set i<old> <new>" > /tmp/red4ext_go` | Find every address holding int `<old>`, write `<new>`, and hold it. **Use the stat's exact current value** as `<old>`. Best when the value is **unique** (e.g. money). |
| `echo "find i<cur>" > /tmp/red4ext_go` | **Delta-narrow start** — snapshot every address holding `<cur>` (no write). Use when the value is **common/small** and `set` refuses it. |
| `echo "narrow <new>" > /tmp/red4ext_go` | After you **change** the stat in-game, keep only the candidates that now hold `<new>`. Repeat find-change-narrow until "few left". |
| `echo "setcand <value>" > /tmp/red4ext_go` | Write `<value>` to the narrowed survivors (revertable, held). Refused while too many candidates remain — narrow more. |
| `echo "i<val>" > /tmp/red4ext_go` | **Identify**: mark each address holding `<val>` with a distinct number `400, 401, …` so you can read the on-screen value to learn which cell is which. |
| `echo "revert" > /tmp/red4ext_go` | Restore all originals and idle. (`echo 0` does the same.) |

`i` = raw integer counter (money/components/XP). A bare number (no `i`) scans player StatsContainer
floats — for derived stats, which generally can't be set (see above).

### Example A — set money directly (unique value)

1. Open your inventory, read your exact eddies (e.g. `310915`).
2. `echo "set i310915 1000000" > /tmp/red4ext_go`
3. Check your eddies — now `1,000,000`.
4. `echo "revert" > /tmp/red4ext_go` — back to `310,915`.

### Example B — delta-narrow (common/small value, or to be precise)

1. `echo "find i310915" > /tmp/red4ext_go` → e.g. `12 candidates`.
2. **Change it in-game** — buy/sell something so it reads, say, `310910`.
3. `echo "narrow 310910" > /tmp/red4ext_go` → e.g. `12 -> 1 candidate`.
4. (Repeat 2–3 if more than a few remain.)
5. `echo "setcand 1000000" > /tmp/red4ext_go` → now `1,000,000`.
6. `echo "revert" > /tmp/red4ext_go` — restore.

---

## Safety notes & limits

- **Stored counters only.** If a `set` finds nothing, the value isn't stored that way (or it's derived).
- **Use the exact current value.** The tool **refuses** a too-common `<old>` (e.g. `0`, `1`): if it
  matches the cell cap it would scatter-write into unrelated memory, so it bails (this is the FA-014
  crash-class guard).
- **Value caps:** `<new>` is capped (int `2e9`, float `1e7`) and must be non-negative.
- **Version gate:** on a game build whose UUID we haven't validated, writes are **disabled** and the log
  says so. After re-validating offsets on the new build, bless it with
  `TWEAKXL_EXPECTED_UUID=<uuid>` (or `TWEAKXL_SKIP_VERSION_GATE=1` for dev).
- **Re-apply:** a background thread re-writes your set value if the game changes it, so it holds until you
  `revert` (or reload).

---

## Troubleshooting

- *Nothing happens:* confirm you're in-world and `/tmp/red4ext-mac.log` shows `[live-edit] LOOP ready`.
- *`set` found 0 addresses:* the `<old>` value isn't currently in memory — re-read the exact current
  number.
- *`refused: … too common`:* pick a value unique to the stat (your exact money, not a small count).
- *`version gate MISMATCH/ DISABLED`:* the game was patched; offsets need re-validation (see above).
