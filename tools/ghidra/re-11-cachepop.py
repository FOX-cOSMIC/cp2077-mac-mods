from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
di = DecompInterface(); di.openProgram(currentProgram)
mon = ConsoleTaskMonitor()
def decompfn(fn):
    r = di.decompileFunction(fn, 120, mon)
    if r and r.getDecompiledFunction(): return r.getDecompiledFunction().getC()
    return None
def fnat(addr):
    return getFunctionContaining(toAddr(addr))

# Who calls the core stat getter FUN_103a7b8e8 and the container-search helper FUN_103a829a8?
# More important: who WRITES the container (+0x50/+0x60 arrays). Find the insert function.
# The container search helper used by getter:
fn = fnat(0x103a829a8)
print("container-search helper FUN_103a829a8 callers/own:")
c = decompfn(fn); print(c[:1500] if c else "n/a")

# Find functions that read a Stat_Record +0x54 AND write into a container, i.e. the stat-init.
# Search: who calls FUN_103a7b8e8 (the getter)? And find the 'register stat' that reads record value.
print("#"*72)
for tgt in [0x103a7b8e8]:
    f=fnat(tgt)
    callers={}
    for ref in getReferencesTo(f.getEntryPoint()):
        cf=getFunctionContaining(ref.getFromAddress())
        if cf: callers[cf.getEntryPoint().toString()]=cf.getName()
    print("callers of stat-getter %s: %d" % (hex(tgt), len(callers)))
    for k in sorted(callers): print("   %s %s"%(k,callers[k]))
