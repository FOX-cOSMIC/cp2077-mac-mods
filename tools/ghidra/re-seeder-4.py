# -*- coding: utf-8 -*-
# Phase A1 (cont): identify the modifier SOURCE objects. The gather does
#   FUN_10098fea0(&coll, statTypeId); n=FUN_102ac0c70(coll);
#   FUN_102ac0a90(coll, &list);  for each obj in list: read obj+0x54/+0x60 mod arrays
# So decode these: are they pure (resolvable from statTypeId + globals) or do they
# need entity context? What object type is each list element?
from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

di = DecompInterface(); di.openProgram(currentProgram)
mon = ConsoleTaskMonitor()

def decomp(addr, lo=0, hi=4000):
    fn = getFunctionContaining(toAddr(addr))
    if not fn: return ("NOFN", None, None)
    r = di.decompileFunction(fn, 180, mon)
    if r and r.getDecompiledFunction():
        return (fn.getName(), r.getDecompiledFunction().getC()[lo:hi], fn)
    return (fn.getName(), None, fn)

for label, a in [("map-stattype->coll", 0x10098fea0),
                 ("coll-count",         0x102ac0c70),
                 ("coll-tolist",        0x102ac0a90),
                 ("array-resolve",      0x100a261ac)]:
    nm,c,fn = decomp(a, hi=3500)
    print("="*78); print("%s  %s = %s" % (label, hex(a), nm)); print("-"*78)
    print(c)
