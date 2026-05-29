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

# hash bucket lookup helper
nm,c,fn=decomp(0x102b139b8)
print("="*72); print("HASH-LOOKUP %s = %s" % (hex(0x102b139b8),nm)); print("-"*72)
if c: print(c[:2500])

# Callers of GetFlat 0x102b76708 - list them with names
fn = getFunctionContaining(toAddr(0x102b76708))
print("\n"+"#"*72)
print("CALLERS of GetFlat FUN_102b76708:")
callers={}
for ref in getReferencesTo(fn.getEntryPoint()):
    cf=getFunctionContaining(ref.getFromAddress())
    if cf: callers[cf.getEntryPoint().toString()]=cf.getName()
for k in sorted(callers): print("  %s %s" % (k, callers[k]))
