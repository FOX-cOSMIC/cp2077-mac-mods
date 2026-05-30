# rw-4: GetFlat decomp (confirm +0x40 array + +0x148 buffer + offset packing),
# find writers of flat-buffer fields (+0x148/+0x150/+0x158/+0x00) and unk160(+0x160),
# and the loader FUN_102253964 that fills flats+buffer. Also FlatValue vtables.
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

# GetFlat - confirm uses +0x40 and +0x148
nm,c,fn=decomp(0x102b76708)
print("="*78); print("GetFlat %s = %s" % (hex(0x102b76708),nm)); print("-"*78)
if c: print(c[:4000])

# AddFlat path: who else calls the binary-search helper FUN_102b139b8 (besides GetFlat)?
fn2=fn_at(0x102b139b8)
seen={}
for ref in getReferencesTo(fn2.getEntryPoint()):
    cf=getFunctionContaining(ref.getFromAddress())
    if cf: seen[cf.getEntryPoint().toString()]=cf.getName()
print("\nbinary-search FUN_102b139b8 callers:", seen)

# loader populate routine
nm,c,fn=decomp(0x102253964, 250)
print("="*78); print("LOADER FUN_102253964 = %s" % nm); print("-"*78)
if c: print(c[:9000])
