# rw-9: decompile the flats/buffer section parser FUN_102b15ec0 -> SetFlatDataBuffer
# (db+0x148/0x150/0x158/0x00) and per-type FlatValue vtable construction; check unk160(+0x160).
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
def callees(fn):
    out=set()
    body=fn.getBody()
    from ghidra.program.model.symbol import RefType
    for ins in currentProgram.getListing().getInstructions(body, True):
        for r in ins.getReferencesFrom():
            if r.getReferenceType().isCall():
                f=getFunctionContaining(r.getToAddress())
                if f: out.add(f.getName()+"@"+f.getEntryPoint().toString())
    return out

nm,c,fn=decomp(0x102b15ec0, 320)
print("="*78); print("FLAT-SECTION PARSER FUN_102b15ec0 = %s" % nm); print("-"*78)
if c: print(c[:12000])
