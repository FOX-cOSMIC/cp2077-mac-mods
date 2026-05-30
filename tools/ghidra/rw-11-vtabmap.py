# rw-11: extract per-type FlatValue vtable assignments from FUN_102b1422c by scanning
# its instructions for 'store of a PTR_ data ptr to [reg]' preceded by the bump-alloc.
# Also resolve each PTR_thunk target's pointed vtable address, and check unk160.
from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
from ghidra.program.model.symbol import RefType
di = DecompInterface(); di.openProgram(currentProgram)
mon = ConsoleTaskMonitor()
listing = currentProgram.getListing()
mem = currentProgram.getMemory()
st = currentProgram.getSymbolTable()

def fn_at(a): return getFunctionContaining(toAddr(a))

# Walk FUN_102b1422c instructions: collect data references to symbols named PTR_thunk/PTR_FUN
fn = fn_at(0x102b1422c)
print("Scanning FlatValue ctor FUN_102b1422c for vtable data refs...")
vtabs=[]
for ins in listing.getInstructions(fn.getBody(), True):
    for r in ins.getReferencesFrom():
        ta=r.getToAddress()
        if ta is None: continue
        sym=st.getPrimarySymbol(ta)
        nm = sym.getName() if sym else ""
        if nm.startswith("PTR_thunk") or (nm.startswith("PTR_FUN") and r.getReferenceType().isData()):
            vtabs.append((ins.getAddress().toString(), ta.toString(), nm))
# dedup keep order
seen=set(); out=[]
for a in vtabs:
    if a[1] in seen: continue
    seen.add(a[1]); out.append(a)
print("Distinct PTR vtable data refs in ctor (insn, vtableAddr, sym):")
for a in out:
    # read pointer at vtable addr (the actual vtable -> first slot is dtor)
    try:
        p = mem.getLong(toAddr(int(a[1],16)))
        print("  %s -> %s  %s   [*]=0x%x" % (a[0], a[1], a[2], p & 0xffffffffffffffff))
    except:
        print("  %s -> %s  %s" % a)

# unk160: search all references to db+0x160. We do it via decompiled GetFlat sibling: look for
# constant 0x160 stores in TweakDB ctor FUN_102b73b50.
print("\n--- unk160 search: decompile TweakDB ctor FUN_102b73b50 (init of +0x160) ---")
r = di.decompileFunction(fn_at(0x102b73b50), 200, mon)
if r and r.getDecompiledFunction():
    c=r.getDecompiledFunction().getC()
    for line in c.splitlines():
        if '0x160' in line or '+ 0x160' in line or '0x158' in line or '0x150' in line or '0x148' in line:
            print("   ", line.strip())
