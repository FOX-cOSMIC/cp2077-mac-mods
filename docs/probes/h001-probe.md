# H-001 Probe — TweakDB Dispatch Table Investigation

**Tests:** H-001 (TweakDB uses a function-pointer dispatch table, not raw data storage)
**Source:** `src/red4ext-mac/src/probes/h001_probe.cpp`
**Launch:** `tools/run-h001-probe.sh`
**Log:** `/tmp/h001-probe.log`

## What It Captures

1. **Image base + ASLR slide** for Cyberpunk2077 binary — lets you convert live addresses to Ghidra offsets: `ghidra_addr = live_addr - slide`.

2. **`__TEXT` / `__DATA` / `__DATA_CONST` segment bounds** — used to identify code pointers and to locate writeable hook targets.

3. **LC_SYMTAB scan** — walks the in-memory symbol table (via LINKEDIT) looking for names containing `TweakDB`, `GetFlat`, `SetFlat`, `flatData`, etc. Commercial binaries are typically stripped; a miss here is expected, not an error.

4. **TweakDB getter symbol lookup** via `dlsym` — tries several mangled name candidates for `RED4ext::TweakDB::Get()`. Finding the symbol confirms it is exported; its address lets you set a Ghidra breakpoint or GOT hook later.

5. **Singleton struct dump (if non-null)** — 64 qwords starting at the TweakDB singleton pointer. Each entry is printed as a hex value; entries that fall inside `__TEXT` are flagged `CODE` and their first 16 bytes (raw, in hex) are printed. A `CODE` entry in the struct is a function pointer, not data storage.

6. **`__DATA` and `__DATA_CONST` structural dump** — always runs regardless of whether the singleton is null. Dumps the first 64 qwords of each writable segment and flags `CODE` entries. This captures compile-time-initialized function-pointer tables that exist in the binary before any runtime initialization — the key signal for H-001.

## Timing and Expected Output

The dyld image-load callback fires **before `main()`**, before any game C++ static constructors run. At this point `TweakDB::Get()` almost certainly returns `null`. This is expected and is logged as:

```
[H001]   singleton = 0x0
[H001]   (null — expected; TweakDB not initialized at image-load time)
```

The valuable findings are still in the log:
- Whether the symbol exists (`dlsym -> FOUND` vs. `not found`)
- The symbol address (for Ghidra cross-reference)
- The image base + slide

If the symbol is missing entirely (`No TweakDB getter symbol found`), the binary strips exports and a Ghidra-based scan is needed instead — hand off to **researcher**.

## Reading the Log

```
[H001] 1748469600.123456789  dylib loaded          ← timestamp (Unix epoch)
[H001] ========== Cyberpunk2077 binary detected ==========
[H001]   base    = 0x0000000102800000               ← mach_header address
[H001]   slide   = 0x0000000002800000               ← ASLR slide
[H001]   __TEXT  = [0x102800000, 0x12a400000)  size=415744 KB
[H001]   __DATA  = vmaddr=0x12a400000  size=... KB
[H001] --- symtab scan ---
[H001]   [symtab] no TweakDB-related symbols in ... scanned entries   ← normal for stripped binary
[H001] --- singleton getter probe ---
[H001]   dlsym(_ZN6RED4ext7TweakDB3GetEv ...) not found              ← expected if stripped
[H001]   no TweakDB getter exported — binary likely stripped
[H001] --- __DATA dump (first 64 qwords @ 0x12a400000) ---
[H001]   [+0000] 0x0000000000000000
[H001]   [+0008] 0x000000010ab3c200  CODE vmaddr=0x1008bc200 | f9 4f be a9 ... |  ← function pointer!
...
[H001]   => 3 code pointer(s) in 64 qwords
```

To convert a live address to a Ghidra vmaddr:
```
ghidra_addr = live_addr - slide
```

A `CODE` entry in a `__DATA` or `__DATA_CONST` dump is a writable function pointer — exactly the kind of hook target we want for H-001.

## What Constitutes a Useful Result

| Outcome | Next step |
|---|---|
| Symbol found + singleton null | Normal. Note symbol address; add a second probe that hooks post-init. |
| Symbol not found | Binary strips exports. Switch to Ghidra scan for `TweakDB::Get`. Hand off to researcher. |
| Symbol found + singleton non-null | Bonus: dump shows function pointers → H-001 likely confirmed. |
| Singleton dump: many `CODE` entries | Strong evidence for function-pointer dispatch table. Hand off to researcher to identify each slot. |
| Singleton dump: no `CODE` entries | Struct is raw data or intra-`__DATA` pointers. H-001 less likely. |
| `__DATA` / `__DATA_CONST` dump: `CODE` entries at consistent offsets | Compile-time-initialized dispatch table found. Note offsets and vmaddrs for Ghidra cross-reference. |
| All dumps: zero `CODE` entries | Table is runtime-initialized (zeros at load time). Need a later-stage probe. Hand off to researcher. |

## Build Only (without launching the game)

```bash
cd $HOME/Programming/cp2077-mac-mods
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target h001_probe
# output: build/src/red4ext-mac/libh001_probe.dylib
```
