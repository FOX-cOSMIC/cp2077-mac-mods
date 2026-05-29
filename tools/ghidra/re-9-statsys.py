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

# StatsDataSystem native query handlers
for a in [0x103832688, 0x1038327ac, 0x10386c7ec]:
    nm,c,fn=decomp(a)
    print("="*72); print("STAT-QUERY-NATIVE %s = %s" % (hex(a),nm)); print("-"*72)
    if c: print(c[:3500])
