# rw-2: find CreateTDBRecord = the single caller of the dispatchers; confirm it
# inserts into +0x58/+0x88. Also decompile the flats binary-search helper
# FUN_102b139b8, and find writers of flatDataBuffer fields (+0x148/+0x150/+0x158).
from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
di = DecompInterface(); di.openProgram(currentProgram)
mon = ConsoleTaskMonitor()

def fn_at(a): return getFunctionContaining(toAddr(a))
def decomp(a, n=160):
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

# 1) Who calls the 4 dispatchers?
DISPATCH=[0x102726504,0x10274f578,0x102798288,0x1027052ac]
callset={}
for a in DISPATCH:
    fn=fn_at(a)
    cs=callers(fn)
    print("dispatcher %s callers: %s" % (hex(a), cs))
    for k,v in cs.items(): callset[k]=v
print("UNION of dispatcher callers:", callset)

# 2) decompile each unique caller (likely the master factory selector / CreateTDBRecord)
for k in sorted(set(callset.keys())):
    a=int(k,16)
    nm,c,fn=decomp(a,200)
    print("="*78); print("CALLER %s = %s  callers=%d" % (k,nm,len(callers(fn))))
    print("-"*78)
    if c: print(c[:9000])
