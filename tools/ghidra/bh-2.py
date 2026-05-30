# -*- coding: utf-8 -*-
# Resolve PTR_FUN_* thunks to actual vtable addresses, read vtable+0x110 slot,
# decompile that GetTweakBaseHash function, and check the returned constant.
from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
di = DecompInterface(); di.openProgram(currentProgram)
mon = ConsoleTaskMonitor()
mem = currentProgram.getMemory()
st = currentProgram.getSymbolTable()
fm = currentProgram.getFunctionManager()

def rd64(a):
    return mem.getLong(toAddr(a)) & 0xffffffffffffffff

def dec_at(a, label):
    f = getFunctionContaining(toAddr(a))
    if not f:
        print("   [%s] no func at 0x%x" % (label, a)); return
    r = di.decompileFunction(f, 120, mon)
    c = r.getDecompiledFunction()
    print("   [%s] func %s @0x%x:" % (label, f.getName(), f.getEntryPoint().getOffset()))
    if c:
        for ln in c.getC().splitlines():
            s=ln.strip()
            if s: print("        "+s)

def sym_addr(name):
    syms = st.getSymbols(name)
    for s in syms:
        return s.getAddress().getOffset()
    return None

# The factory writes *rec = &PTR_FUN_XXXX. That &PTR_FUN is a label on a pointer
# slot that holds the actual vtable address. So vtableAddr = rd64(PTR_FUN_addr).
# But often &PTR_FUN_10704f2e8 already IS the vtable (its bytes are function ptrs).
# Determine: read [PTR+0x08]=GetNativeType (slot1), and [PTR+0x110]=GetTweakBaseHash.

# candidates: (label, PTR_FUN static addr as used in factory)
cands = [
  ("Stat?(.statType/.value 0x1885c3e0)", 0x10704f2e8),
  ("case0 .icons 0x5b303040", 0x10704f678),
  ("StatModifier?(.statType/.modifierType -0x430cadde)", 0x107057400),
]
for label, ptr in cands:
    print("\n=== %s : PTR @0x%x ===" % (label, ptr))
    # Is &PTR_FUN the vtable itself, or a pointer to it?
    slot0 = rd64(ptr)
    print("   [*]PTR (slot0/dtor) = 0x%x" % slot0)
    # treat ptr as the vtable base
    slot1 = rd64(ptr+0x08)   # GetNativeType
    slot110 = rd64(ptr+0x110)
    print("   vtbl+0x08 (GetNativeType) = 0x%x" % slot1)
    print("   vtbl+0x110 (GetTweakBaseHash candidate) = 0x%x" % slot110)
    dec_at(slot110, label+" +0x110")
    # also probe a couple nearby slots in case +0x110 wrong
    for off in (0x108, 0x118):
        s = rd64(ptr+off)
        print("   vtbl+0x%x = 0x%x" % (off, s))
