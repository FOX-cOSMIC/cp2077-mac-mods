# -*- coding: utf-8 -*-
# Find the engine INSTANCE singleton global: a __bss/__common/__data pointer that
# is WRITTEN once and READ in many places, where the written value is a freshly
# constructed object. Heuristic: scan all functions for the pattern
#   str Xthis,[Gslot]    (store a non-null pointer to a global)
# specifically inside the engine init. Instead: find globals that are both
# (a) written exactly in 1-2 funcs and (b) read then deref +0x308 anywhere.
# Simpler: scan for instructions "ldr R,[R,#0x308]" -> get function -> decompile
# -> textual search for "+ 0x308) + 0x10". Then within that function find the
# source of the engine pointer (global or arg).
from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
di=DecompInterface(); di.openProgram(currentProgram); mon=ConsoleTaskMonitor()
fm=currentProgram.getFunctionManager()
listing=currentProgram.getListing()
mem=currentProgram.getMemory()
# collect funcs that have a ldr at +0x308 (data load, not vtable call form)
cand=set()
for f in fm.getFunctions(True):
    for i in listing.getInstructions(f.getBody(),True):
        if i.getMnemonicString()=="ldr" and "#0x308]" in i.toString():
            cand.add(f)
            break
print("funcs with ldr [..,#0x308]: %d"%len(cand))
# decompile each, look for "0x308) + 0x10" textual chain (framework->gameInstance)
hitfns=[]
for f in cand:
    r=di.decompileFunction(f,40,mon)
    if not (r and r.getDecompiledFunction()): continue
    c=r.getDecompiledFunction().getC()
    if "0x308) + 0x10" in c or ("+ 0x308)" in c and "+ 0x10)" in c):
        hitfns.append((f.getEntryPoint().getOffset(),f.getName(),c))
print("funcs whose decompile shows +0x308 then +0x10 chain: %d"%len(hitfns))
for ent,nm,c in hitfns[:12]:
    print("  0x%x %s"%(ent,nm))
# print the first few decompiles (trim)
for ent,nm,c in hitfns[:5]:
    print("\n##### %s @0x%x #####"%(nm,ent))
    # print only lines containing 0x308 / 0x10 / global DAT
    for line in c.split("\n"):
        if "0x308" in line or ("+ 0x10)" in line) or "DAT_" in line or "PTR_" in line:
            print("   "+line.strip())
