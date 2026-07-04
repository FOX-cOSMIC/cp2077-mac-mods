# macOS Code Signing & Entitlements — Canonical Reference

**Scope:** Re-signing the Cyberpunk 2077 binary so `DYLD_INSERT_LIBRARIES` injection works on macOS with Hardened Runtime enabled.

**Maintained by:** `doc-keeper` (Ledger)
**Related:** F-001, FA-001, H-004, T-001, T-005

---

## Background

Apple's Hardened Runtime (enabled for all notarized apps since macOS 10.15) enforces strict protections on signed binaries:

- `__TEXT` segments are read-execute only — `mprotect()` and `vm_protect()` return EPERM (see FA-001).
- `DYLD_INSERT_LIBRARIES` is silently ignored unless the binary explicitly opts in via entitlements.
- Dynamic linker environment variables are stripped at exec unless the binary declares them safe.

The practical consequence: we cannot inject a dylib into Cyberpunk 2077 without first re-signing the binary with four specific entitlements. The game's original Apple Developer signature must be replaced with an ad-hoc signature that carries those entitlements.

---

## Current Build Status (per F-001)

> **Game build 2.3.1 — re-signing is NOT required for v1.0 work.**

The stock binary shipped with game build 2.3.1 already carries two of the four entitlements:

- `com.apple.security.cs.disable-library-validation` ✓ present
- `com.apple.security.cs.allow-dyld-environment-variables` ✓ present

These two are sufficient for basic `DYLD_INSERT_LIBRARIES` injection. On this build, injecting our dylib works out of the box — no re-signing, no entitlement edits needed.

The remaining two entitlements (`allow-unsigned-executable-memory` and `allow-jit`) are **absent** from the stock binary. They are only required for `MAP_JIT` trampoline pages. That work is deferred to v1.x (ARM64 inline hooking); it is not needed for the v1.0 scope (TweakXL mod loading via `DYLD_INSERT_LIBRARIES`).

**This is build-specific.** A future game update may ship a binary with these entitlements stripped, restoring the need to re-sign. The full Re-Sign Procedure below remains the correct response if that happens. Run the stale-signature check before each development session (see §"What Game Updates Do").

---

## The Four Required Entitlements

All four must be present. Missing any one will cause injection to fail or the game to crash.

| Entitlement key | Rationale |
|---|---|
| `com.apple.security.cs.disable-library-validation` | Permits dylib injection; without this, `DYLD_INSERT_LIBRARIES` is silently ignored even when the env var is set. |
| `com.apple.security.cs.allow-dyld-environment-variables` | Allows the `DYLD_*` environment-variable family to be passed into the process at all; Hardened Runtime strips them otherwise. |
| `com.apple.security.cs.allow-unsigned-executable-memory` | Permits our trampoline pages (and any heap-allocated code stubs) to be marked executable via `mmap(MAP_ANON)`; required for ARM64 hook trampolines. |
| `com.apple.security.cs.allow-jit` | Enables `MAP_JIT` on `mmap` calls, which provides a writable-then-executable region for JIT-compiled or self-patching code; needed for any `rwx` trampoline approach. |

### Optional fifth — for inline `__TEXT` patching (Phase 5 fallback)

> **⚠ CORRECTED 2026-07-04 by F-053.** The claim below ("a re-sign carrying this entitlement lifts [`__TEXT` immutability]") was never empirically tested when written and is now known to be **wrong as stated**, at least for ad-hoc signing. `h004_probe.cpp` tested this entitlement (plus the four above, plus `get-task-allow`) against 4 distinct ad-hoc-resigned copies of the real game binary: `mach_vm_protect`/`mprotect` on `__TEXT` failed identically (KERN_PROTECTION_FAILURE / EPERM) in every configuration, including this one. See **F-053** for full methodology. The remaining untested lever is a real (non-ad-hoc) Apple Developer ID signature — not ruled out by this experiment, but a materially higher-cost path (see H-012's discussion of memaxo's own analysis).

| Entitlement key | Rationale |
|---|---|
| `com.apple.security.cs.disable-executable-page-protection` | Disables Hardened Runtime's protection on **existing** executable pages, allowing in-place modification of the game's `__TEXT` (true inline hooking). This is what scopes **FA-001**: the stock binary's `__TEXT` is immutable, but a re-sign carrying this entitlement lifts that. It is the **most security-reducing** entitlement Apple ships (explicitly discouraged) and is **not** in the four above — add it only if the GOT/VTable/`MAP_JIT`-trampoline routes can't intercept a needed direct call. See `docs/ROADMAP.md` Phase 5. **UPDATE: empirically refuted for ad-hoc signing — see the correction note above and F-053.** |

Entitlement plist fragment for reference (the four required; add the fifth only for inline `__TEXT` hooking):

```xml
<key>com.apple.security.cs.disable-library-validation</key>
<true/>
<key>com.apple.security.cs.allow-dyld-environment-variables</key>
<true/>
<key>com.apple.security.cs.allow-unsigned-executable-memory</key>
<true/>
<key>com.apple.security.cs.allow-jit</key>
<true/>
```

---

## Re-Sign Procedure

### Prerequisites

- Xcode Command Line Tools: `xcode-select --install`
- Disk space: ~1 GB free (binary is ~600 MB; we keep one backup)
- The game must not be running.

### Step 1 — Define paths

```bash
GAME_DIR="$HOME/Library/Application Support/Steam/steamapps/common/Cyberpunk 2077/Cyberpunk2077.app"
BIN="$GAME_DIR/Contents/MacOS/Cyberpunk2077"
ENT_FILE="/tmp/cp2077-entitlements.plist"
BACKUP="$BIN.pre-resign-backup"
```

### Step 2 — Back up the binary

Do this every time, even if a backup already exists. If the game has been updated since the last resign, the "old" backup may now be a different game version — rename it with a datestamp first.

```bash
# If a backup already exists, version it before overwriting
[[ -f "$BACKUP" ]] && mv "$BACKUP" "${BACKUP}.$(date +%Y%m%d)"

cp "$BIN" "$BACKUP"
echo "Backed up to: $BACKUP"
ls -lh "$BACKUP"
```

### Step 3 — Extract current entitlements

```bash
codesign --display --entitlements :- "$BIN" > "$ENT_FILE"
echo "Extracted entitlements to: $ENT_FILE"
cat "$ENT_FILE"
```

If the binary has no entitlements at all, `codesign` may output nothing or produce an empty plist. In that case, start from a minimal skeleton:

```bash
cat > "$ENT_FILE" << 'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
</dict>
</plist>
EOF
```

### Step 4 — Merge the four entitlements into the plist

Open `$ENT_FILE` in any text editor and add the four `<key>/<true/>` pairs inside the `<dict>` block. Do not remove any entitlements that were already present (e.g., `com.apple.security.network.client`).

> **⚠ EXCEPTION (added 2026-07-04, per F-053): DO remove CDPR's *restricted*, Apple-provisioned entitlements before ad-hoc re-signing.** Empirically, preserving `com.apple.application-identifier`, `com.apple.developer.team-identifier`, and `com.apple.developer.sustained-execution` (all present on the stock binary, all tied to CDPR's real Apple Developer provisioning) while signing ad-hoc (`--sign -`) causes the process to be **SIGKILL'd instantly at launch (exit 137, no crash report)** — confirmed directly, not theoretical. These three keys are the one category of "already present" entitlement that must be dropped, not kept, when re-signing ad-hoc. The four/five entitlements this doc adds are unaffected (they are ordinary developer opt-outs, not Apple-provisioned identifiers) and launch fine once the restricted keys are excluded.

Alternatively, use PlistBuddy:

```bash
/usr/libexec/PlistBuddy \
  -c "Add :com.apple.security.cs.disable-library-validation bool true" \
  -c "Add :com.apple.security.cs.allow-dyld-environment-variables bool true" \
  -c "Add :com.apple.security.cs.allow-unsigned-executable-memory bool true" \
  -c "Add :com.apple.security.cs.allow-jit bool true" \
  "$ENT_FILE" 2>/dev/null || true
# PlistBuddy exits non-zero if the key already exists — that's fine, suppress it.
```

### Step 5 — Re-sign with ad-hoc identity

```bash
codesign \
  --force \
  --sign - \
  --entitlements "$ENT_FILE" \
  --options runtime \
  "$BIN"
echo "Re-sign exit code: $?"
```

Flag notes:
- `--force` replaces any existing signature without prompting.
- `--sign -` is the ad-hoc identity (a dash, not a certificate). Steam and the game do not care about the signing identity — only the entitlements matter for our purposes.
- `--options runtime` preserves the Hardened Runtime flag; some older guides omit this, which silently disables the entitlements.

### Step 6 — Verify

```bash
# Display the entitlements on the re-signed binary
codesign --display --entitlements :- "$BIN"

# Confirm the four keys are present
for key in \
  "com.apple.security.cs.disable-library-validation" \
  "com.apple.security.cs.allow-dyld-environment-variables" \
  "com.apple.security.cs.allow-unsigned-executable-memory" \
  "com.apple.security.cs.allow-jit"
do
  grep -q "$key" <(codesign --display --entitlements :- "$BIN" 2>&1) \
    && echo "OK: $key" \
    || echo "MISSING: $key"
done

# Basic signature validity
codesign --verify --deep --strict "$BIN" && echo "Signature valid"
```

All four keys must show `OK`. If any show `MISSING`, repeat from Step 4.

---

## What Game Updates Do

Steam and GOG updates **replace the binary on disk**, which overwrites our ad-hoc signature with the publisher's original one. The original signature does not include the four entitlements. After any update:

- `DYLD_INSERT_LIBRARIES` will be silently ignored again.
- The framework dylib will not load.
- The game will appear to run normally (no crash) but with no mod support.

### Detecting a stale signature

```bash
BIN="$HOME/Library/Application Support/Steam/steamapps/common/Cyberpunk 2077/Cyberpunk2077.app/Contents/MacOS/Cyberpunk2077"

codesign --display --entitlements :- "$BIN" 2>&1 \
  | grep -q "disable-library-validation" \
  && echo "OK — binary has been re-signed" \
  || echo "STALE — binary needs re-signing"
```

Run this check at the start of any mod-enabled game session, or wire it into the game launcher. If it prints `STALE`, re-run the full Step 2–6 procedure above.

### Detecting an update via binary hash

Record the SHA256 after each re-sign. If it changes, assume the binary was replaced:

```bash
# Save fingerprint after resign
shasum -a 256 "$BIN" > "${BIN}.sha256"

# Check before each session
shasum -a 256 --check "${BIN}.sha256" && echo "Binary unchanged" || echo "Binary replaced — re-sign needed"
```

---

## Troubleshooting

### `DYLD_INSERT_LIBRARIES` is silently ignored

**Symptom:** Dylib injection command runs without error; game launches; dylib `_attribute_((constructor))` never fires; nothing in crash logs.

**Causes and checks:**

1. `disable-library-validation` is missing from the entitlements.
   → Run the verification in Step 6. If the key is absent, repeat Steps 4–5.

2. `allow-dyld-environment-variables` is missing.
   → Same check and fix.

3. The binary was updated since the last re-sign.
   → Run the stale-signature check above.

4. SIP (System Integrity Protection) is blocking injection for protected paths.
   → The Steam game path is not SIP-protected. If you moved the game to `/Applications/`, it may be. Keep the game in the Steam library path.

5. The injected dylib is itself a fat binary or has a code-signing issue.
   → Test with a minimal hello-world dylib first to isolate the variable.

### Immediate crash on launch after re-sign

**Symptom:** Game crashes within ~1 second of launch; no log output from our dylib; crash report shows `EXC_BAD_ACCESS` or `SIGKILL`.

**Causes and checks:**

1. `--options runtime` was omitted from the `codesign` call.
   → This silently drops the Hardened Runtime flag; macOS may then refuse to execute the binary. Re-sign with `--options runtime` included.

2. The entitlements plist is malformed XML.
   → Validate with: `plutil -lint "$ENT_FILE"`. Fix any errors and re-sign.

3. The binary was signed but the `.app` bundle's `_CodeSignature/CodeResources` is now inconsistent (deep signing required).
   → If resources are checked: `codesign --force --sign - --entitlements "$ENT_FILE" --options runtime --deep "$GAME_DIR"` (sign the whole app). Note this takes longer.

4. macOS Gatekeeper quarantine flag is set.
   → `xattr -d com.apple.quarantine "$BIN"` and retry.

### `mprotect()` returns EPERM on `__TEXT` pages

**Symptom:** Probe dylib calls `mprotect(addr, size, PROT_READ|PROT_WRITE|PROT_EXEC)` on a `__TEXT` address; it returns `-1` with `errno = 13` (EPERM).

**This is expected behavior.** See FA-001.

The `allow-jit` and `allow-unsigned-executable-memory` entitlements do **not** grant write access to `__TEXT` pages. Code signing enforces `__TEXT` immutability at the kernel level regardless of entitlements. There is no entitlement that bypasses this on Apple Silicon.

**What to do:** Do not attempt to write to `__TEXT`. Use `__DATA`-resident hook targets only — GOT entries, VTable slots, function-pointer tables. These are writable and unaffected by code signing. See AGENTS.md constraint #1 and FA-001.

### `codesign` reports "no identity found" or certificate errors

**Symptom:** `codesign --force --sign MyCert --entitlements ...` fails with an identity error.

**Cause:** You are not using the ad-hoc identity (`-`). We do not need a real certificate. Use `--sign -` (a literal dash).

---

## Alternative Tooling: `rcodesign`

[`rcodesign`](https://github.com/indygreg/apple-platform-rs/tree/main/apple-codesign) is a Rust-based tool that can re-sign Mach-O binaries without Xcode. It is useful in CI environments or non-Mac hosts. The equivalent command:

```bash
rcodesign sign \
  --entitlements-xml-file "$ENT_FILE" \
  "$BIN"
```

`rcodesign` does not require an Apple Developer account for ad-hoc signing. Use it as a fallback if Xcode CLT is unavailable. We have not validated `rcodesign` against the current game build — if you use it, verify with `codesign --display --entitlements` afterwards.

---

## Script Outline for `tools/resign-game.sh`

The following is a prose outline of what the script should do. **Do not write the script yet** — this outline captures the requirements for when a hook-engineer or tester implements it.

```
tools/resign-game.sh — outline (not yet implemented)

PURPOSE
  Detect whether the game binary needs re-signing and re-sign it if so.
  Safe to run before every game session; a no-op if nothing changed.

INPUTS
  - (Optional) --force flag to skip the staleness check and always re-sign
  - Game directory auto-detected from well-known Steam/GOG paths, or
    overridden by GAME_DIR env var

STEPS
  1. Locate the game binary. Abort with a clear error if not found.
  2. Check if the binary is already re-signed:
       codesign --display --entitlements - "$BIN" | grep -q disable-library-validation
     If signed and not --force: print "already signed, nothing to do" and exit 0.
  3. Back up the binary with a datestamped name. If an existing backup
     is from the same SHA256 as the current binary, skip the backup
     (nothing changed since last re-sign).
  4. Extract current entitlements to a temp file.
  5. Merge the four required entitlements using PlistBuddy (idempotent — skip
     keys that already exist).
  6. Re-sign: codesign --force --sign - --entitlements <tmp> --options runtime "$BIN"
  7. Verify: check all four keys present; run codesign --verify.
     If any check fails, restore from backup and exit non-zero.
  8. Print a summary: game version (from Info.plist), SHA256 of the new binary,
     entitlements confirmed.

ERROR HANDLING
  - Any step that fails should restore the backup and exit non-zero with a
    clear human-readable message.
  - Never leave the binary in a partially-signed state.

GAME VERSION LOGGING
  - Always emit the bundle version from Info.plist in the output.
    This makes it trivial to correlate a re-sign event with a specific game build.
```

---

## Cross-References

| Reference | Relevance |
|---|---|
| FA-001 | `__TEXT` hooking via mprotect: why it fails, and why re-signing does not fix it |
| H-004 | Active hypothesis: inline `__TEXT` hooking still blocked on current build |
| T-001 | Researcher task: confirm current binary is correctly signed as part of baseline capture |
| T-005 | Researcher task: smoke-test `mprotect` EPERM on current build |
| AGENTS.md constraint #1 | Project-level prohibition on inline `__TEXT` hooking |
