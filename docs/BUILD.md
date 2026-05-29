# BUILD.md

## Current State

**There is nothing to build yet.** This project is in Phase 0 (re-validation). The CMake skeleton is in place but `src/` is empty.

This file documents the intended build flow so future agents can populate it as code lands.

---

## Prerequisites

- macOS 12+ on Apple Silicon (M1/M2/M3/M4)
- Xcode Command Line Tools (`xcode-select --install`)
- CMake 3.15+
- Cyberpunk 2077: Ultimate Edition installed (Steam / GOG / Mac App Store)
- The game binary must be re-signed with `disable-library-validation` and `allow-jit` entitlements (see `docs/reference/macos-codesigning.md`)

---

## Build

```bash
cd /Users/lucas_1/Programming/cp2077-mac-mods

# Configure
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug

# Build all targets
cmake --build build

# Build just red4ext-mac
cmake --build build --target red4ext

# Build just tweakxl-mac
cmake --build build --target tweakxl
```

Outputs:

- `build/src/red4ext-mac/libred4ext.dylib`
- `build/src/tweakxl-mac/libtweakxl.dylib`

---

## Run / Launch the Game with the Framework

Once `libred4ext.dylib` exists:

```bash
# Set the game path
GAME="$HOME/Library/Application Support/Steam/steamapps/common/Cyberpunk 2077/Cyberpunk2077.app/Contents/MacOS/Cyberpunk2077"

# Launch with the dylib injected
env DYLD_INSERT_LIBRARIES="$PWD/build/src/red4ext-mac/libred4ext.dylib" \
    "$GAME" 2>&1 | tee /tmp/cp2077-modded.log
```

For non-interactive smoke tests (game launches, runs for N seconds, gets killed):

```bash
./tools/smoke-test.sh 30   # run for 30 seconds, then kill
```

---

## Tests

```bash
# Unit tests
cmake --build build --target test

# Or directly via ctest
cd build && ctest --output-on-failure
```

---

## Plugin Layout

Once red4ext-mac can load plugins, they go in:

```
~/Library/Application Support/Steam/steamapps/common/Cyberpunk 2077/Cyberpunk2077.app/Contents/PlugIns/red4ext/plugins/
├── tweakxl/
│   └── libtweakxl.dylib
└── <other>/
    └── <plugin>.dylib
```

`tools/install.sh` will handle copying built dylibs into this location.

---

## Re-Signing the Game Binary

See `docs/reference/macos-codesigning.md` for full details. Quick reference:

```bash
GAME_DIR="$HOME/Library/Application Support/Steam/steamapps/common/Cyberpunk 2077/Cyberpunk2077.app"
BIN="$GAME_DIR/Contents/MacOS/Cyberpunk2077"

# 1. Extract current entitlements
codesign -d --entitlements :- "$BIN" > /tmp/cp2077-ent.plist

# 2. Edit /tmp/cp2077-ent.plist to add:
#    com.apple.security.cs.disable-library-validation = true
#    com.apple.security.cs.allow-dyld-environment-variables = true
#    com.apple.security.cs.allow-unsigned-executable-memory = true
#    com.apple.security.cs.allow-jit = true

# 3. Re-sign
codesign --force --sign - --entitlements /tmp/cp2077-ent.plist "$BIN"

# 4. Verify
codesign --display --entitlements - "$BIN"
```

This is a destructive change to the game install; back up the original `Cyberpunk2077` binary before re-signing.

---

## Troubleshooting

### `DYLD_INSERT_LIBRARIES` is ignored

→ Game binary needs `disable-library-validation` and `allow-dyld-environment-variables` entitlements. Re-sign per above.

### Game crashes immediately on launch

→ Check `~/Library/Logs/DiagnosticReports/` for a `Cyberpunk2077-*.ips` file. Parse with the snippet in `.openclaw/agents/tester.md`.

### `mprotect` fails in our code

→ You're probably trying to write to `__TEXT`. Read `docs/FAILED_APPROACHES.md` (FA-001). Use a `__DATA` hook strategy instead.

### Hook fires but no behavior change

→ See FA-007. You may have hooked the storage path, not the dispatch path. Check that your hook is on a function-pointer table entry, not raw data.
