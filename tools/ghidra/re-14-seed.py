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

for a in [0x103a99a60, 0x103a9b03c, 0x103a9b73c]:
    nm,c,fn=decomp(a)
    print("="*72); print("STAT-SEED %s = %s" % (hex(a),nm)); print("-"*72)
    if c: print(c[:4200])
    # callers - is this called once-per-entity (init) or per-query?
    callers={}
    for ref in getReferencesTo(fn.getEntryPoint()):
        cf=getFunctionContaining(ref.getFromAddress())
        if cf: callers[cf.getEntryPoint().toString()]=cf.getName()
    print("   callers:", len(callers), [k for k in sorted(callers)][:10])
