# Phase 1: xref the singleton global, classify callers, find +0x58 (flats) readers.
from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

di = DecompInterface()
di.openProgram(currentProgram)
mon = ConsoleTaskMonitor()

def decomp(fn):
    r = di.decompileFunction(fn, 60, mon)
    if r and r.getDecompiledFunction():
        return r.getDecompiledFunction().getC()
    return None

print("=== XREFS to singleton global 0x1080c92d0 ===")
seen = {}
refs = getReferencesTo(toAddr(0x1080c92d0))
cnt = 0
for ref in refs:
    cnt += 1
    fa = ref.getFromAddress()
    fn = getFunctionContaining(fa)
    name = fn.getName() if fn else "?"
    ent = fn.getEntryPoint() if fn else None
    key = ent.toString() if ent else name
    seen.setdefault(key, (name, 0))
    seen[key] = (name, seen[key][1]+1)
print("total xref sites: %d ; distinct functions: %d" % (cnt, len(seen)))
for k in sorted(seen.keys()):
    print("  fn %s  %s  (xrefs in fn: %d)" % (k, seen[k][0], seen[k][1]))
