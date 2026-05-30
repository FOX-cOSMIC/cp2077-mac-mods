# rw-5: flats Insert path FUN_102b11e48; xref writers of flat-buffer fields;
# unk160 readers; FlatValue vtable for Float/Int32/Bool via registry FUN_102b777e8.
from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
di = DecompInterface(); di.openProgram(currentProgram)
mon = ConsoleTaskMonitor()

def fn_at(a): return getFunctionContaining(toAddr(a))
def decomp(a, n=200):
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

# flats Insert candidate
nm,c,fn=decomp(0x102b11e48, 220)
print("="*78); print("FLATS-INSERT? FUN_102b11e48 = %s callers=%d" % (nm,len(callers(fn))))
print("-"*78)
if c: print(c[:6000])

# the flat-type-id registry (int=2,float=3,bool=4) - maps type->vtable?
nm,c,fn=decomp(0x102b777e8, 220)
print("="*78); print("FLAT-TYPE REGISTRY FUN_102b777e8 = %s" % nm); print("-"*78)
if c: print(c[:6000])
