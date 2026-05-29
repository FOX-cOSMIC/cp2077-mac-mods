from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
di = DecompInterface(); di.openProgram(currentProgram)
mon = ConsoleTaskMonitor()
def decomp(addr):
    fn = getFunctionContaining(toAddr(addr))
    if not fn: return ("NOFN", None, None)
    r = di.decompileFunction(fn, 120, mon)
    if r and r.getDecompiledFunction(): return (fn.getName(), r.getDecompiledFunction().getC(), fn)
    return (fn.getName(), None, fn)

# THE flat lookup by id
nm,c,fn=decomp(0x102b76708)
print("="*72); print("GETFLAT-BY-ID %s = %s entry=%s" % (hex(0x102b76708),nm, fn.getEntryPoint())); print("-"*72)
if c: print(c[:5000])

# how many callers?
callers={}
for ref in getReferencesTo(fn.getEntryPoint()):
    cf=getFunctionContaining(ref.getFromAddress())
    if cf: callers[cf.getEntryPoint().toString()]=cf.getName()
print("\nDISTINCT CALLERS of FUN_102b76708 (GetFlat): %d" % len(callers))
