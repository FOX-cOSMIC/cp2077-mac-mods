# rw-1: WRITE-side survey. Decompile candidate CreateTDBRecord dispatchers,
# the flats binary-search helper, and map xrefs to the flat-buffer fields.
from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
di = DecompInterface(); di.openProgram(currentProgram)
mon = ConsoleTaskMonitor()

def fn_at(a):
    return getFunctionContaining(toAddr(a))

def decomp(a, n=120):
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

CANDS = [0x102726504, 0x10274f578, 0x102798288, 0x1027052ac]
for a in CANDS:
    nm,c,fn = decomp(a)
    print("="*78)
    print("CAND %s = %s  callers=%d" % (hex(a), nm, len(callers(fn)) if fn else -1))
    print("-"*78)
    if c: print(c[:6000])
