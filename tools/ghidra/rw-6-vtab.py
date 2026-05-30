# rw-6: find AllocateFlatValue/CreateFlatValue (per-type FlatValue vtable placement),
# unk160 gate, and the CreateTDBRecord caller FUN_102b16a48 (load loop / UpdateRecord-equiv).
from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
di = DecompInterface(); di.openProgram(currentProgram)
mon = ConsoleTaskMonitor()

def fn_at(a): return getFunctionContaining(toAddr(a))
def decomp(a, n=240):
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

# CreateTDBRecord other caller: the load loop / update path
nm,c,fn=decomp(0x102b16a48, 240)
print("="*78); print("FUN_102b16a48 = %s callers=%d" % (nm,len(callers(fn)))); print("-"*78)
if c: print(c[:9000])

# Look for the FlatValue allocator: search functions that reference db+0x158 (flatDataBufferEnd)
# We approach via the 'Float'/'Int32' cstrings used by AllocateFlatValue switch, if present.
import ghidra.program.model.symbol as S
mem = currentProgram.getMemory()
# Find a function that does placement-new of FlatValue: it writes a vtable then data at +0x10.
# Strategy: list xrefs to the flat-type registry globals would be heavy; instead inspect
# FUN_1026b8ce8 (the OTHER caller of CreateTDBRecord) which may be CreateRecord wrapper.
nm,c,fn=decomp(0x1026b8ce8, 200)
print("="*78); print("FUN_1026b8ce8 = %s callers=%d" % (nm,len(callers(fn)))); print("-"*78)
if c: print(c[:5000])
