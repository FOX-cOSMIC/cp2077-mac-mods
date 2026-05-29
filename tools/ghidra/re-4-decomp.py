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

# Prime GetFlat candidates: touch both +0x58 (flats map) and +0x148 (value buffer)
for a in [0x102726504, 0x10274f578, 0x102798288, 0x1027052ac]:
    nm,c = decomp(a)
    print("="*72); print("FUNC %s = %s" % (hex(a), nm)); print("-"*72)
    if c: print(c[:4000])
