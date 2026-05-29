from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

di = DecompInterface()
di.openProgram(currentProgram)
mon = ConsoleTaskMonitor()

def decomp(addr):
    fn = getFunctionContaining(toAddr(addr))
    if not fn: return ("NOFN@"+hex(addr), None)
    r = di.decompileFunction(fn, 90, mon)
    if r and r.getDecompiledFunction():
        return (fn.getName(), r.getDecompiledFunction().getC())
    return (fn.getName(), None)

# Inspect the getter and the two adjacent funcs near 0x1080c92d0 readers
for a in [0x102b73c7c, 0x102b73c1c, 0x102b73b50, 0x102b777e8]:
    nm, c = decomp(a)
    print("="*70)
    print("FUNC @ %s = %s" % (hex(a), nm))
    print("-"*70)
    if c: print(c[:2500])

# Now count callers of the getter 0x102b73c7c
print("#"*70)
fn = getFunctionContaining(toAddr(0x102b73c7c))
print("Getter function entry: %s name=%s" % (fn.getEntryPoint(), fn.getName()))
callers = {}
for ref in getReferencesTo(fn.getEntryPoint()):
    fa = ref.getFromAddress()
    cf = getFunctionContaining(fa)
    if cf:
        callers[cf.getEntryPoint().toString()] = cf.getName()
print("DISTINCT CALLERS of getter: %d" % len(callers))
