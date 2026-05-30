# rw-10: FUN_102b1422c = per-flat FlatValue constructor (placement-new + per-type vtable);
# FUN_1009148cc = buffer-field setter; check unk160 (db+0x160) refs across DB funcs.
from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
di = DecompInterface(); di.openProgram(currentProgram)
mon = ConsoleTaskMonitor()
def fn_at(a): return getFunctionContaining(toAddr(a))
def decomp(a, n=300):
    fn = fn_at(a)
    if not fn: return ("NOFN", None, None)
    r = di.decompileFunction(fn, n, mon)
    if r and r.getDecompiledFunction():
        return (fn.getName(), r.getDecompiledFunction().getC(), fn)
    return (fn.getName(), None, fn)

for a in [0x102b1422c, 0x1009148cc]:
    nm,c,fn=decomp(a, 260)
    print("="*78); print("%s = %s" % (hex(a),nm)); print("-"*78)
    if c: print(c[:9000])
