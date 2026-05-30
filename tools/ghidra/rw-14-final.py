# rw-14: decompile Float FlatValue value-getter (vtable slot @+0x10 -> 0x102b73ccc)
# to confirm scalar data lives at FlatValue+0x08; and dtor slot.
from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
di = DecompInterface(); di.openProgram(currentProgram)
mon = ConsoleTaskMonitor()
def fn_at(a): return getFunctionContaining(toAddr(a))
def decomp(a,n=80):
    fn=fn_at(a)
    if not fn: return("NOFN",None)
    r=di.decompileFunction(fn,n,mon)
    if r and r.getDecompiledFunction(): return (fn.getName(),r.getDecompiledFunction().getC())
    return(fn.getName(),None)

for label,a in [("Float-getter slot2",0x102b73ccc),("Float-vt slot1",0x10096c6d8),
                ("Float-vt slot0(dtor)",0x10096c6d4),("Int32-getter? slot2",0x102b73cd4)]:
    nm,c=decomp(a,80)
    print("="*70); print("%s -> %s = %s" % (label,hex(a),nm)); print("-"*70)
    if c: print(c[:1500])
