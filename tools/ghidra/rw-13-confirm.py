# rw-13: (a) confirm FlatValue scalar data offset by inspecting a GetFlatValue<float>
# wrapper (a caller of GetFlat FUN_102b76708) reading FlatValue+0x08;
# (b) dump the Float vtable (4 slots) for cloning; (c) hunt unk160 globally via singleton xref.
from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
di = DecompInterface(); di.openProgram(currentProgram)
mon = ConsoleTaskMonitor()
mem=currentProgram.getMemory()
def fn_at(a): return getFunctionContaining(toAddr(a))
def decomp(a,n=120):
    fn=fn_at(a)
    if not fn: return("NOFN",None,fn)
    r=di.decompileFunction(fn,n,mon)
    if r and r.getDecompiledFunction(): return (fn.getName(),r.getDecompiledFunction().getC(),fn)
    return(fn.getName(),None,fn)

# (a) a few GetFlat callers - look for '+ 8' deref of returned ptr (scalar read)
gf=fn_at(0x102b76708)
cs=[]
for ref in getReferencesTo(gf.getEntryPoint()):
    f=getFunctionContaining(ref.getFromAddress())
    if f: cs.append(f.getEntryPoint().getOffset())
cs=sorted(set(cs))[:4]
for a in cs:
    nm,c,fn=decomp(a,120)
    print("="*70); print("GetFlat caller %s = %s" % (hex(a),nm)); print("-"*70)
    if c:
        for ln in c.splitlines():
            if '76708' in ln or '+ 8)' in ln or '+ 0x10)' in ln or 'return' in ln:
                print("   ",ln.strip())

# (b) dump Float vtable @ 0x106e925f0 : read 4 pointers
print("\nFloat FlatValue vtable @ 0x106e925f0 slots:")
for i in range(4):
    try:
        p=mem.getLong(toAddr(0x106e925f0+8*i)) & 0xffffffffffffffff
        print("   [%d] @+0x%x = 0x%x" % (i,8*i,p))
    except Exception as e: print("   [%d] err %s"%(i,e))

# (c) unk160 global hunt: references to singleton then +0x160
from ghidra.program.model.symbol import RefType
print("\nunk160 hunt: functions referencing TweakDB singleton 0x1080c92d0:")
seen=set()
for ref in getReferencesTo(toAddr(0x1080c92d0)):
    f=getFunctionContaining(ref.getFromAddress())
    if f and f.getEntryPoint().getOffset() not in seen:
        seen.add(f.getEntryPoint().getOffset())
print("   singleton xref function count:", len(seen))
