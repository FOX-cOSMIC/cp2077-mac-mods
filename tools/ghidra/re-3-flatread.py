from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
di = DecompInterface(); di.openProgram(currentProgram)
mon = ConsoleTaskMonitor()

def decompfn(fn):
    r = di.decompileFunction(fn, 90, mon)
    if r and r.getDecompiledFunction(): return r.getDecompiledFunction().getC()
    return None

# Among the 1678 callers of the getter, find ones whose body indexes +0x58 (flats map).
# That is the GetFlat / flat-resolve path. We look at the decompiled C for "+ 0x58" near a getter call.
getter = getFunctionContaining(toAddr(0x102b73c7c))
callers = {}
for ref in getReferencesTo(getter.getEntryPoint()):
    cf = getFunctionContaining(ref.getFromAddress())
    if cf: callers[cf.getEntryPoint().toString()] = cf

print("scanning %d callers for +0x58 flat-map access..." % len(callers))
hits58 = []
hits148 = []
n=0
for ent, cf in callers.items():
    n+=1
    c = decompfn(cf)
    if not c: continue
    if "0x58" in c:
        hits58.append((ent, cf.getName(), c.count("0x58")))
    if "0x148" in c:
        hits148.append((ent, cf.getName()))
print("CALLERS touching +0x58: %d" % len(hits58))
for h in sorted(hits58, key=lambda x:-x[2])[:40]:
    print("  %s %s  (0x58 occ=%d)" % h)
print("CALLERS touching +0x148 (flat buffer): %d" % len(hits148))
for h in hits148[:40]:
    print("  %s %s" % h)
