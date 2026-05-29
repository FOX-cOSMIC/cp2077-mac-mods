from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
di = DecompInterface(); di.openProgram(currentProgram)
mon = ConsoleTaskMonitor()
def decomp(addr):
    fn = getFunctionContaining(toAddr(addr))
    if not fn: return ("NOFN",None,None)
    r = di.decompileFunction(fn, 120, mon)
    if r and r.getDecompiledFunction(): return (fn.getName(), r.getDecompiledFunction().getC(), fn)
    return (fn.getName(),None,fn)

# record-by-id resolver used by stats - confirm it reads +0x58 map of singleton
nm,c,fn=decomp(0x102628104)
print("="*72); print("RECORD-BY-ID %s = %s" % (hex(0x102628104),nm)); print("-"*72)
if c: print(c[:2200])

# The orchestrator
nm,c,fn=decomp(0x103a9aa8c)
print("="*72); print("ORCHESTRATOR %s = %s" % (hex(0x103a9aa8c),nm)); print("-"*72)
if c: print(c[:1500])
callers={}
for ref in getReferencesTo(fn.getEntryPoint()):
    cf=getFunctionContaining(ref.getFromAddress())
    if cf: callers[cf.getEntryPoint().toString()]=cf.getName()
print("   orchestrator callers:", len(callers), [k for k in sorted(callers)][:12])
