# rw-8: hunt FlatValue allocator + per-type vtables + unk160.
from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
di = DecompInterface(); di.openProgram(currentProgram)
mon = ConsoleTaskMonitor()

def fn_at(a): return getFunctionContaining(toAddr(a))
def decomp(a, n=260):
    fn = fn_at(a)
    if not fn: return ("NOFN", None, None)
    r = di.decompileFunction(fn, n, mon)
    if r and r.getDecompiledFunction():
        return (fn.getName(), r.getDecompiledFunction().getC(), fn)
    return (fn.getName(), None, fn)
def callers(fn):
    out={}
    for ref in getReferencesTo(fn.getEntryPoint()):
        cf=getFunctionContaining(ref.getFromAddress())
        if cf: out[cf.getEntryPoint().toString()]=cf.getName()
    return out

# FUN_102b13d04 -> the record/query builder caller; look for buffer + flatvalue creation
for a in [0x102b13d04, 0x102b7d228, 0x102b7cc9c]:
    nm,c,fn=decomp(a, 220)
    print("="*78); print("%s = %s callers=%d" % (hex(a),nm,len(callers(fn)))); print("-"*78)
    if c: print(c[:6500])
