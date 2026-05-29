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

# Look at a few GetFlat callers that are NOT obviously record ctors - the higher-addr ones
# These could be live consumers. Decompile compact, look at how the returned buffer ptr is used.
for a in [0x100988634, 0x10135cf38, 0x101381bd0, 0x1014d2334, 0x10328b108, 0x1023ae84c]:
    nm,c,fn=decomp(a)
    print("="*72); print("CALLER %s = %s" % (hex(a),nm)); print("-"*72)
    if c: print(c[:1800])
