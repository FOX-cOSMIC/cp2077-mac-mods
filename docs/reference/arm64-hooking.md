# ARM64 Hooking on macOS — Canonical Reference

**Scope:** How to hook game functions on Apple Silicon macOS without touching `__TEXT`. All strategies described here operate exclusively on writable `__DATA` memory.

**Maintained by:** `doc-keeper` (Ledger)
**Related:** FA-001, H-004, T-005, `docs/reference/macos-codesigning.md`, AGENTS.md constraint #1

---

## 1. Why No Inline `__TEXT` Hooking

The classic x86/ARM technique — overwrite the first bytes of a function with a jump to a hook — does not work on macOS with Hardened Runtime. `__TEXT` pages are immutable: `mprotect(PROT_WRITE)` and `vm_protect(VM_PROT_WRITE)` both return permission errors at the kernel level, regardless of entitlements. (See FA-001 for the full incident record.)

**There is no entitlement that grants write access to `__TEXT`.** `allow-jit` and `allow-unsigned-executable-memory` apply only to `mmap`-allocated pages, not to the loader-mapped `__TEXT` segment.

All hook strategies in this project target `__DATA` segment memory: GOT entries, vtable pointer slots, and function-pointer table entries. These are writable by default.

See H-004 for the open task to smoke-test this constraint on the current game build (T-005).

---

## 2. ARM64 Absolute-Jump Sequence (20 bytes, 5 instructions)

ARM64 has no x86-style `JMP rel32`. Relative branch instructions (`B`, `BL`) have a ±128 MB range, which is insufficient when jumping from a `MAP_JIT` page to an arbitrary function in `__TEXT` (addresses can be >4 GB apart). The canonical solution is a 5-instruction sequence loading a 64-bit absolute address into `x16` then branching:

```
MOVZ x16, #(addr >>  0) & 0xFFFF              ; bits 0–15
MOVK x16, #(addr >> 16) & 0xFFFF, LSL #16    ; bits 16–31 (no-zero-fill)
MOVK x16, #(addr >> 32) & 0xFFFF, LSL #32    ; bits 32–47
MOVK x16, #(addr >> 48) & 0xFFFF, LSL #48    ; bits 48–63
BR   x16                                       ; branch to x16
```

`x16` (IP0, Intra-Procedure-call scratch register 0) is reserved by the ARM64 ABI for this exact purpose. It is safe to clobber across a call boundary.

### 2.1 Instruction Encoding

All ARM64 instructions are 32 bits wide, fixed width, little-endian in memory.

**MOVZ Xd, #imm16, LSL #(hw×16)**

```
[31] [30:29] [28:23]  [22:21] [20:5]  [4:0]
 sf    opc   100101     hw    imm16    Rd

sf=1 (64-bit), opc=10 (MOVZ) → bits 31-29 = 110
```

Encoder expression (verify against ARM ARM, Move wide immediate):
```c
uint32_t movz_x16(uint16_t imm, int hw) {
    // hw ∈ {0,1,2,3} → shift = hw*16
    return 0xD2800010u | ((uint32_t)hw << 21) | ((uint32_t)imm << 5);
}
```

Base opcode for each shift (x16, imm=0):
| Shift | Base opcode | `hw` bits |
|-------|-------------|-----------|
| LSL #0  | `0xD2800010` | 00 |
| LSL #16 | `0xD2A00010` | 01 |
| LSL #32 | `0xD2C00010` | 10 |
| LSL #48 | `0xD2E00010` | 11 |

**MOVK Xd, #imm16, LSL #(hw×16)**

Same layout as MOVZ but `opc=11` (bits 31-29 = 111):
```c
uint32_t movk_x16(uint16_t imm, int hw) {
    return 0xF2800010u | ((uint32_t)hw << 21) | ((uint32_t)imm << 5);
}
```

Base opcodes for each shift (x16, imm=0):
| Shift | Base opcode |
|-------|-------------|
| LSL #0  | `0xF2800010` |
| LSL #16 | `0xF2A00010` |
| LSL #32 | `0xF2C00010` |
| LSL #48 | `0xF2E00010` |

**BR x16**

```
[31:25]   [24] [23:21] [20:16] [15:10] [9:5]  [4:0]
1101011    0    000    11111  000000    Rn    00000

Rn = 16 (x16) → [9:5] = 10000
```

Fixed encoding: `0xD61F0200` *(verify against ARM ARM, Unconditional branch (register))*

### 2.2 Encoder — Complete 5-Instruction Sequence

```c
void emit_abs_jump(uint32_t* out, uint64_t target) {
    out[0] = 0xD2800010u | (((target >>  0) & 0xFFFF) << 5); // MOVZ x16, lo16
    out[1] = 0xF2A00010u | (((target >> 16) & 0xFFFF) << 5); // MOVK x16, bits 16-31
    out[2] = 0xF2C00010u | (((target >> 32) & 0xFFFF) << 5); // MOVK x16, bits 32-47
    out[3] = 0xF2E00010u | (((target >> 48) & 0xFFFF) << 5); // MOVK x16, bits 48-63
    out[4] = 0xD61F0200u;                                     // BR   x16
}
```

This writes exactly 20 bytes. The caller is responsible for flushing the i-cache after writing (see §6).

---

## 3. Trampoline Layout

A trampoline lets the hook call the original function (rather than replacing it entirely). It consists of:

1. A copy of the first N bytes of the original function (the "stolen" bytes) — enough to make room for the hook in the original location, without conflicting with whatever we're hooking in `__DATA`.
2. An absolute jump back to `original + N`.

```
┌────────────────────────────────────────────────┐
│  TRAMPOLINE PAGE (MAP_JIT, RWX)                │
│                                                 │
│  +0x00  ┌─────────────────────────────────┐    │
│         │  copied instruction(s) from     │    │
│         │  original function prologue     │    │
│         │  (N bytes, relocated if needed) │    │
│         └─────────────────────────────────┘    │
│  +N     ┌─────────────────────────────────┐    │
│         │  MOVZ x16, lo16                 │    │
│         │  MOVK x16, bits 16-31           │    │  ──► original_fn + N
│         │  MOVK x16, bits 32-47           │    │
│         │  MOVK x16, bits 48-63           │    │
│         │  BR   x16                       │    │
│         └─────────────────────────────────┘    │
└────────────────────────────────────────────────┘
```

**N must be a multiple of 4** (ARM64 instructions are 4 bytes each). Steal enough instructions so that the space freed in the original function can hold the hook (e.g., if hooking a vtable slot pointing to the function, no bytes need to be stolen at all — just replace the pointer). For function-entry hooks via trampoline, steal ≥5 instructions (20 bytes) to make room for the absolute-jump in the original.

---

## 4. Position-Dependent Instruction Relocation

When copying instructions from `__TEXT` to a trampoline page, the PC (program counter) changes. Instructions that encode a PC-relative offset will target the wrong address unless rewritten.

### 4.1 Instructions That MUST Be Relocated

| Instruction | Encoding | Range | Rewrite Strategy |
|---|---|---|---|
| `B label` | 26-bit signed PC-relative imm (×4) | ±128 MB | If new PC puts label out of range, rewrite as MOVZ/MOVK/BR sequence. Otherwise update the imm. |
| `BL label` | Same as `B` | ±128 MB | Same as `B` but emit `BLR x16` instead of `BR x16` to preserve link register semantics. |
| `B.cond label` | 19-bit signed PC-relative imm (×4) | ±1 MB | Short form rarely survives relocation. Rewrite as: `B.cond +8; B skip; MOVZ/MOVK/BR` or invert condition + jump-over. |
| `CBZ/CBNZ Rn, label` | 19-bit signed PC-relative imm (×4) | ±1 MB | Similar to `B.cond`. Rewrite with conditional + absolute branch. |
| `TBZ/TBNZ Rn, #bit, label` | 14-bit signed PC-relative imm (×4) | ±32 KB | Very short range. Nearly always needs rewrite. Use same pattern as `B.cond`. |
| `ADR Xd, label` | 21-bit signed PC-relative imm (byte-addressed) | ±1 MB | Rewrite: compute absolute address at emit time, emit `MOVZ/MOVK` into `Xd` (4–5 instructions). |
| `ADRP Xd, label` | 21-bit signed PC-relative imm (×4096, page-aligned) | ±4 GB | Rewrite: compute absolute page address at emit time, emit `MOVZ/MOVK` for the page + preserve any `ADD`/`LDR` that follows using the same `Xd`. Note: `ADRP` is almost always followed by `ADD Xd, Xd, #lo12` or `LDR Xd, [Xd, #lo12]` — relocate the pair together. |
| `LDR Xd, label` (literal) | 19-bit signed PC-relative imm (×4) | ±1 MB | Rewrite: emit `MOVZ/MOVK` into a scratch register for the source address, then `LDR Xd, [scratch]`. |

### 4.2 Instructions That Are Safe to Copy Verbatim

- `LDR`/`STR`/`LDP`/`STP` with register base (`[Xn, #imm]` or `[Xn, Xm]`)
- All ALU instructions (`ADD`, `SUB`, `AND`, `ORR`, `EOR`, `MOV` between registers, etc.)
- `MOV`, `MOVN` with immediate (not PC-relative)
- `MRS`/`MSR` (system register accesses)
- `BLR`, `BR`, `RET` (branch to register — already absolute)
- Anything that does not encode an immediate relative to PC

### 4.3 Minimum Stolen Instruction Count

A safe minimum is 5 instructions (20 bytes) to create space for the 5-instruction absolute jump, unless the entire prologue before the first return can fit in fewer. Scan instructions one-by-one; do not split a multi-instruction group (e.g., an `ADRP`+`ADD` pair must be stolen together or not at all).

---

## 5. MAP_JIT and W^X on Apple Silicon

### 5.1 Allocating Executable Pages

```c
#include <sys/mman.h>
#include <pthread.h>  // for pthread_jit_write_protect_np

size_t page_size = (size_t)getpagesize();

void* trampoline_page = mmap(
    NULL,
    page_size,
    PROT_READ | PROT_WRITE | PROT_EXEC,
    MAP_ANON | MAP_PRIVATE | MAP_JIT,
    -1, 0
);

if (trampoline_page == MAP_FAILED) {
    // Check errno. Common failures:
    //   EPERM  → allow-jit entitlement missing
    //   EINVAL → MAP_JIT not supported (pre-macOS 10.14)
}
```

**Required entitlement:** `com.apple.security.cs.allow-jit`. Without it, `mmap(MAP_JIT)` returns `EPERM`. See `docs/reference/macos-codesigning.md`.

### 5.2 W^X Per-Thread Toggle (Apple Silicon Only)

Apple Silicon enforces write-XOR-execute (W^X) on a per-thread basis for `MAP_JIT` pages. A thread can either write to the page or execute from it, not both simultaneously. Toggle before and after each write:

```c
// Before writing trampoline code:
pthread_jit_write_protect_np(0);   // 0 = enable writes, disable execute

emit_abs_jump((uint32_t*)trampoline_page, target_address);
// ... write any other code to the page ...

// After writing:
pthread_jit_write_protect_np(1);   // 1 = enable execute, disable writes

// Flush caches AFTER re-enabling execute:
__builtin___clear_cache(trampoline_page, (char*)trampoline_page + bytes_written);
```

**Important:** On Intel Macs, `MAP_JIT` pages are truly RWX and `pthread_jit_write_protect_np` is a no-op. Code that omits the toggle works on Intel but segfaults on Apple Silicon. Always include the toggle.

`pthread_jit_write_protect_np` is defined in `<pthread.h>` on macOS 11+. For earlier targets, conditionalize on `__arm64__` and OS version.

---

## 6. Instruction Cache Flushing

After writing code to any page, the CPU's data cache (D-cache) and instruction cache (I-cache) may be out of sync. The I-cache may hold stale bytes from before the write. **Always flush:**

```c
__builtin___clear_cache(
    (char*)trampoline_start,
    (char*)trampoline_start + bytes_written
);
```

This is a GCC/Clang intrinsic that emits the correct `DC CVAU` + `DSB ISH` + `IC IVAU` + `DSB ISH` + `ISB` instruction sequence on ARM64 — exactly what the architecture requires for I-cache coherence.

**When to call it:**
- After writing any trampoline code (including the initial population of the page).
- After patching a GOT entry or vtable slot (these are data writes, not code; but if any code reads the pointer and caches it in a register, that's the caller's concern — the flush is not needed for pure pointer writes).
- After any `pthread_jit_write_protect_np(1)` transition — call `__builtin___clear_cache` after re-enabling execute, not before.

**What happens without it:** The CPU executes whatever bytes were in the I-cache before the write. Usually a crash. On Apple Silicon the symptom is often `SIGILL` on an instruction that looks correct in the debugger (because the debugger reads memory, not the I-cache).

---

## 7. Hook Types

### 7.1 GOT Entry Hook

The Global Offset Table (`.got` section, in `__DATA`) holds resolved absolute addresses for external symbols. The dynamic linker fills these at load time. We can overwrite a GOT entry to redirect any call through it.

**When to use:** When the game calls a known external symbol (e.g., a libc function, a framework function) that we want to intercept. Also useful when RE reveals an indirect call through a GOT-like table.

**Procedure:**
1. Find the GOT entry address (symbol + offset, or via `dlsym` + `_GLOBAL_OFFSET_TABLE_`).
2. Save the original pointer (for the trampoline).
3. Write our hook function's address into the GOT entry using a plain store — no code patching, no cache flush needed for pure data writes.

```c
void** got_entry = /* address of GOT slot */;
void* original   = *got_entry;         // save for trampoline
*got_entry       = (void*)my_hook;     // redirect
```

### 7.2 VTable Slot Hook

C++ objects with virtual functions have a vtable — an array of function pointers in `__DATA`. Each instance holds a pointer to its class's vtable. Overwriting a slot redirects all virtual calls on that class.

**When to use:** When we need to intercept a virtual method on TweakDB or another game object. H-003 / T-004 test VTABLE[5] as a startup-fire trigger.

**Procedure:**
1. Obtain a pointer to the live object instance.
2. Dereference the vptr (first field) to get the vtable base address.
3. Write the hook's address to the desired slot index.

```c
void**  vtable   = *(void***)instance;          // dereference vptr
void*   original = vtable[slot_index];          // save
vtable[slot_index] = (void*)my_hook;            // redirect
```

**Caution:** Vtable slots are shared across all instances of the class. Hooking one instance hooks all. Also, vtable layouts change with game updates (see FA-005, FA-006). Always validate slot indices against the current game build via T-004 before shipping code that depends on a specific slot number.

### 7.3 Function-Pointer Table Hook

Some subsystems use a flat array of function pointers (neither vtable nor GOT). H-001 posits that TweakDB's `staticFlatDataBuffer` is such a table. T-002 will validate this.

**When to use:** When RE reveals a `__DATA` array of pointers that the game dispatches through at runtime, and we want to intercept specific entries.

**Procedure:** Same as vtable hook — find the base address of the table, compute the slot offset, save and overwrite.

**Identifying function-pointer tables:** When Ghidra shows a `__DATA` array where entries point into `__TEXT` (code addresses), and the game calls them via `LDR Xn, [base, #offset]; BLR Xn` patterns, that's a function-pointer table.

---

## 8. Cross-References

| Reference | Relevance |
|---|---|
| FA-001 | `__TEXT` hooking via mprotect — why it fails; the foundational constraint |
| H-004 / T-005 | Open task: smoke-test mprotect EPERM on current build |
| H-001 / T-002 | Open task: validate TweakDB function-pointer dispatch table (§7.3) |
| H-003 / T-004 | Open task: validate VTABLE[5] as startup-fire hook (§7.2) |
| AGENTS.md constraint #1 | Project-level prohibition on inline `__TEXT` hooking |
| `docs/reference/macos-codesigning.md` | Re-sign procedure; `allow-jit` entitlement (§5) |
| ARM Architecture Reference Manual (A64 ISA) | Authoritative bit-level encoding for all instructions in §2 |
