# rw-3: confirm FUN_102b74408 inserts into +0x58/+0x88; decompile flats
# binary-search helper FUN_102b139b8; find writers of flat-buffer fields and unk160.
from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
from ghidra.program.model.address import GenericAddress
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

for a in [0x102b74408, 0x102b139b8]:
    nm,c,fn=decomp(a)
    print("="*78); print("FUNC %s = %s callers=%d" % (hex(a),nm,len(callers(fn))))
    print("-"*78)
    if c: print(c[:7000])

# CreateTDBRecord (FUN_1026b8db8) -- who calls it? (UpdateRecord-equiv / loader)
fn=fn_at(0x1026b8db8)
print("="*78); print("CreateTDBRecord FUN_1026b8db8 callers:", callers(fn))
