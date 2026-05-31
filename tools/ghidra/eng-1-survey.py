# -*- coding: utf-8 -*-
from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
di = DecompInterface(); di.openProgram(currentProgram)
mon = ConsoleTaskMonitor()
def dec(addr,name,t=120,lim=4000):
    f=getFunctionContaining(toAddr(addr))
    if not f: print("no func @%x"%addr); return None
    r=di.decompileFunction(f,t,mon)
    c=r.getDecompiledFunction()
    print("\n##### %s (%s @ entry 0x%x) #####"%(name,f.getName(),f.getEntryPoint().getOffset()))
    if c:
        s=c.getC()
        print(s[:lim])
        return f
    print("<fail>"); return f

# 1. Known consumers from F-037 context
dec(0x103fd1c18,"GetLocalPlayerMainGameObject (re-9 ref)")
# 2. The StatsDataSystem query native handlers - they reach gameInstance->GetSystem
for a in [0x103832688, 0x1038327ac]:
    dec(a,"stat-query-native "+hex(a))
