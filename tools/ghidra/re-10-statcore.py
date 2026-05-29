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

for a in [0x103a7b8e8, 0x103a82ba4]:
    nm,c,fn=decomp(a)
    print("="*72); print("STAT-CORE %s = %s" % (hex(a),nm)); print("-"*72)
    if c: print(c[:6000])
    # does it call GetFlat or the singleton getter?
    if c:
        for needle in ["FUN_102b76708","FUN_102b73c7c","0x54","+ 0x54)"]:
            print("   contains %s : %s" % (needle, needle in c))
