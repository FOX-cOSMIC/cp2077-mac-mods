# rw-7: locate the FlatValue allocator + per-type vtables, and unk160 gate.
# Approach: find functions referencing the FlatValue allocation. The DB-load populate
# builds flats; the buffer base is db+0x148, end db+0x158. We scan the loader subtree.
# Also: dump the per-type FlatValue vtables by reading an existing Float flat's vtable
# is a runtime thing; statically we look for the placement-new sites.
from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
di = DecompInterface(); di.openProgram(currentProgram)
mon = ConsoleTaskMonitor()
fm = currentProgram.getFunctionManager()

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

# The flats-insert/query function FUN_102b11e48 had ONE caller. Decompile it - it's the
# group/query builder OR the load populate that also creates flat VALUES.
fn=fn_at(0x102b11e48)
print("FUN_102b11e48 caller(s):", callers(fn))

# The single caller of FUN_102b16a48 chain: who reserves and loops? climb to find buffer setup.
for a in [0x102b16a48]:
    print("FUN_102b16a48 callers:", callers(fn_at(a)))

# Find functions that read/write db+0x160 (unk160) and db+0x158 (bufEnd) and db+0x150(cap):
# Decompile FUN_102b75744 orchestrator and FUN_102b75908 region to see buffer alloc + unk160 init.
nm,c,fn=decomp(0x102b75744, 300)
print("="*78); print("ORCHESTRATOR FUN_102b75744 = %s" % nm); print("-"*78)
if c: print(c[:9000])
