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

# Scan whole stats TU for the function that reads a gamedataStat_Record value at +0x54
# and seeds the container. Strategy: enumerate funcs in 0x103a70000-0x103ad0000 that contain
# both "+ 0x54)" and a write into a container-like +0xc/+0x60. Too many; instead decompile
# FUN_103a85dc8 (returns the float* slot) and find its callers that pass a record value.
nm,c,fn=decomp(0x103a85dc8)
print("="*72); print("MODIFIER-SLOT %s = %s" % (hex(0x103a85dc8),nm)); print("-"*72)
if c: print(c[:2500])
callers={}
for ref in getReferencesTo(fn.getEntryPoint()):
    cf=getFunctionContaining(ref.getFromAddress())
    if cf: callers[cf.getEntryPoint().toString()]=cf.getName()
print("callers of FUN_103a85dc8:", len(callers))
for k in sorted(callers): print("  ",k,callers[k])

# Also: the function that READS Stat_Record default. Search the stats TU functions that call
# the singleton getter FUN_102b73c7c AND read +0x54.
print("#"*72)
gt = getFunctionContaining(toAddr(0x102b73c7c))
# Find getter-callers in stats range that also reference 0x54
import re
stats_lo, stats_hi = 0x103820000, 0x103b00000
cand=[]
for ref in getReferencesTo(gt.getEntryPoint()):
    cf=getFunctionContaining(ref.getFromAddress())
    if cf:
        ep=cf.getEntryPoint().getOffset()
        if stats_lo<=ep<stats_hi:
            cand.append(cf)
seen=set(); 
print("getter-callers in STATS range:", len(cand))
for cf in cand:
    ep=cf.getEntryPoint().toString()
    if ep in seen: continue
    seen.add(ep)
    cc=decomp(cf.getEntryPoint().getOffset())[1]
    if cc and "+ 0x54)" in cc:
        print("  STAT-RECORD-READER:", ep, cf.getName())
