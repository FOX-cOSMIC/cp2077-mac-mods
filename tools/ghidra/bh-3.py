# -*- coding: utf-8 -*-
# Scan the Stat record vtable (PTR_FUN_10704f2e8) slot-by-slot. For each slot,
# decompile the target and look for a function that returns the constant 0x1885c3e0
# (the factory CONST for the .statType/.value type). Report the slot offset.
from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
import re
di = DecompInterface(); di.openProgram(currentProgram)
mon = ConsoleTaskMonitor()
mem = currentProgram.getMemory()

def rd64(a): return mem.getLong(toAddr(a)) & 0xffffffffffffffff

VTBL = 0x10704f2e8
TARGET = 0x1885c3e0  # factory CONST for this type

# how many slots? vtables are in .const; find run of valid code pointers.
def is_code_ptr(p):
    try:
        f = getFunctionContaining(toAddr(p))
        return f is not None
    except:
        return False

print("Scanning vtable @0x%x for slot returning 0x%x" % (VTBL, TARGET))
seen={}
n=0
off=0
while off <= 0x200:
    p = rd64(VTBL+off)
    f = getFunctionContaining(toAddr(p)) if p else None
    if f is None:
        off+=8; continue
    fa = f.getEntryPoint().getOffset()
    if fa not in seen:
        r = di.decompileFunction(f, 60, mon)
        c = r.getDecompiledFunction()
        body = c.getC() if c else ""
        seen[fa]=body
    body = seen[fa]
    # search for the constant in hex (positive and as int)
    hexs = "%x" % TARGET
    hit = (hexs in body.lower()) or (str(TARGET) in body)
    mark = "  <<< RETURNS TARGET" if hit else ""
    # also flag tiny functions that just return a constant
    is_const_ret = ("return 0x" in body or re.search(r'return [0-9]+;', body)) and body.count("return")<=2 and len(body)<400
    cm = "  [const-return]" if is_const_ret else ""
    print("  +0x%03x -> 0x%x %s @0x%x%s%s" % (off, p, f.getName(), fa, mark, cm))
    if hit:
        print("      ---- BODY ----")
        for ln in body.splitlines():
            if ln.strip(): print("      "+ln.strip())
    off+=8
