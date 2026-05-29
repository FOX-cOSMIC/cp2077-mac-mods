from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
di = DecompInterface(); di.openProgram(currentProgram)
mon = ConsoleTaskMonitor()
def decomp(addr):
    fn = getFunctionContaining(toAddr(addr))
    if not fn: return ("NOFN",None,None)
    r = di.decompileFunction(fn, 120, mon)
    if r and r.getDecompiledFunction(): return (fn.getName(), r.getDecompiledFunction().getC(), fn)
    return (fn.getName(),None,fn)

# GetRecord(TweakDBID) - the +0x58 record lookup
nm,c,fn=decomp(0x102b745d0)
print("="*72); print("GETRECORD %s = %s" % (hex(0x102b745d0),nm)); print("-"*72)
if c: print(c[:3000])
callers={}
for ref in getReferencesTo(fn.getEntryPoint()):
    cf=getFunctionContaining(ref.getFromAddress())
    if cf: callers[cf.getEntryPoint().toString()]=cf.getName()
print("   GetRecord distinct callers:", len(callers))

# climb: orchestrator callers FUN_103a9c758 / FUN_103a9d088 -> who calls them (entity stat init?)
for a in [0x103a9c758, 0x103a9d088]:
    f=getFunctionContaining(toAddr(a))
    cc={}
    for ref in getReferencesTo(f.getEntryPoint()):
        c2=getFunctionContaining(ref.getFromAddress())
        if c2: cc[c2.getEntryPoint().toString()]=c2.getName()
    print("   callers of %s: %d %s" % (hex(a), len(cc), [k for k in sorted(cc)][:8]))
