from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
di = DecompInterface(); di.openProgram(currentProgram)
mon = ConsoleTaskMonitor()
def decomp(addr):
    fn = getFunctionContaining(toAddr(addr))
    if not fn: return ("NOFN", None)
    r = di.decompileFunction(fn, 120, mon)
    if r and r.getDecompiledFunction(): return (fn.getName(), r.getDecompiledFunction().getC())
    return (fn.getName(), None)

# The flat-resolve helper called from record ctors
for a in [0x10096c4c0, 0x1009c0244, 0x100990060]:
    nm,c=decomp(a)
    print("="*72); print("RESOLVER %s = %s" % (hex(a),nm)); print("-"*72)
    if c: print(c[:3500])
