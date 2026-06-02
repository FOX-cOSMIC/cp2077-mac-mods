# reference/ — Read-Only Carry-Overs

These are **symlinks** to original source locations, not copies. Reasons:

- `ghidra/` is ~9.5GB — copying would waste disk space.
- All three are read-only inputs to the project; no agent should write here.
- If the original location moves, update the symlink, don't duplicate.

| Symlink | Points to | Purpose |
|---------|-----------|---------|
| `ghidra/` | `$HOME/Programming/Ghidra/` | Ghidra project + exported C decomp |
| `example-mods/` | `$HOME/Programming/adtional-modding-data/` | Real Nexus mods, modding support files |
| `windows-tweakxl/` | `$HOME/Programming/OLD/cp2077-tweak-xl/` | psiberx's original TweakXL source (compatibility reference) |

## Rules

- **Read-only.** No agent writes to these paths. Treat them as immutable inputs.
- If a Ghidra finding is important, **copy the relevant snippet** into `docs/FACTS.md` or `docs/reference/` — don't link into the Ghidra project from production code.
- If Windows TweakXL source informs an implementation choice, **link the specific file + commit** in the relevant DECISIONS.md entry — don't write code that depends on the symlink resolving.

## If You Need to Update the Originals

The originals live at the symlink targets. The cp2077-mac-mods project should never directly modify them.
