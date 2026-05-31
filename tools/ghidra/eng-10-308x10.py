# -*- coding: utf-8 -*-
# Find functions that contain BOTH a load at +0x308 and (nearby) a load at +0x10,
# small accessor-like functions (few instructions). These are GetGameInstance/GetFramework.
fm=currentProgram.getFunctionManager()
listing=currentProgram.getListing()
from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
di=DecompInterface(); di.openProgram(currentProgram); mon=ConsoleTaskMonitor()
cands=[]
for f in fm.getFunctions(True):
    body=f.getBody()
    has308=False; has10=False; ninstr=0
    for i in listing.getInstructions(body,True):
        ninstr+=1
        t=i.toString()
        if "#0x308]" in t: has308=True
        if "#0x10]" in t and i.getMnemonicString()=="ldr": has10=True
    if has308 and has10 and ninstr<=40:
        cands.append((f.getEntryPoint().getOffset(),f.getName(),ninstr))
print("small funcs with +0x308 and +0x10 loads: %d"%len(cands))
for ent,nm,n in cands[:30]:
    print("  0x%x %s (%d instr)"%(ent,nm,n))
# decompile the smallest few
cands.sort(key=lambda x:x[2])
for ent,nm,n in cands[:8]:
    f=fm.getFunctionContaining(toAddr(ent))
    r=di.decompileFunction(f,90,mon); c=r.getDecompiledFunction()
    print("\n##### %s @0x%x (%d instr) #####"%(nm,ent,n))
    print(c.getC()[:1800] if c else "<fail>")
