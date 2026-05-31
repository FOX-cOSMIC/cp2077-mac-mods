# -*- coding: utf-8 -*-
from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
di=DecompInterface(); di.openProgram(currentProgram); mon=ConsoleTaskMonitor()
def dec(addr,name,lim=4000):
    f=getFunctionContaining(toAddr(addr))
    if not f: print("no func @%x"%addr); return f
    r=di.decompileFunction(f,150,mon); c=r.getDecompiledFunction()
    print("\n########## %s (%s @0x%x) ##########"%(name,f.getName(),f.getEntryPoint().getOffset()))
    print(c.getC()[:lim] if c else "<fail>")
    return f
# GetGameInstance native handler & GetLocalPlayerMainGameObject native handler
dec(0x103fd1bc8,"GetGameInstance native handler")
dec(0x103fd1d30,"GetLocalPlayerControlledGameObject native")
# vtable+0x10 GetSystem: decompile FUN_101d97f30 (used in site C as FUN_101d97f30(puVar4, type))
dec(0x101d97f30,"FUN_101d97f30 (GetSystem-ish helper, site C)")
