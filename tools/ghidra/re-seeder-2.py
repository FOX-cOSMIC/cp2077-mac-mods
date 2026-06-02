# -*- coding: utf-8 -*-
# Phase A1 (cont): the fold/recompute, the value-read in the gather, and the
# modifier type identity. Find how a modifier record's VALUE is read.
from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

di = DecompInterface(); di.openProgram(currentProgram)
mon = ConsoleTaskMonitor()

def decomp(addr, lo=0, hi=9000):
    fn = getFunctionContaining(toAddr(addr))
    if not fn: return ("NOFN", None, None)
    r = di.decompileFunction(fn, 200, mon)
    if r and r.getDecompiledFunction():
        return (fn.getName(), r.getDecompiledFunction().getC()[lo:hi], fn)
    return (fn.getName(), None, fn)

# 1) tail of modifier-gather (value read after GetRecord resolves the modifier)
nm,c,fn = decomp(0x103a99a60, lo=6000, hi=12000)
print("="*78); print("GATHER-A TAIL  %s" % nm); print("-"*78)
print(c)

# 2) the fold/recompute that writes container+0x60[i]+0xc
nm,c,fn = decomp(0x103a7d4bc, hi=9000)
print("="*78); print("RECOMPUTE-FOLD %s" % nm); print("-"*78)
print(c)

# 3) the modifier type the gather compares against (FUN_102734ec4 returns a type ptr)
for a in [0x102734ec4, 0x1021975bc, 0x102b73c7c]:
    nm,c,fn = decomp(a, hi=1500)
    print("="*78); print("HELPER %s = %s" % (hex(a), nm)); print("-"*78)
    print(c)

# 4) search the type-name hash constant 0x7d83071a369fc395 site context
print("="*78); print("Looking for type-name strings near modifier types"); print("-"*78)
for s in ["ConstantStatModifier","CombinedStatModifier","CurveStatModifier","gamedataConstantStatModifier"]:
    found = []
    it = currentProgram.getListing().getDefinedData(True)
    # cheap: scan defined strings
print("(string scan skipped - use separate query if needed)")
