# -*- coding: utf-8 -*-
from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
di=DecompInterface(); di.openProgram(currentProgram); mon=ConsoleTaskMonitor()
def dec(addr,name,lim=6000):
    f=getFunctionContaining(toAddr(addr))
    if not f: print("no func @%x"%addr); return
    r=di.decompileFunction(f,150,mon); c=r.getDecompiledFunction()
    print("\n########## %s (%s @0x%x) ##########"%(name,f.getName(),f.getEntryPoint().getOffset()))
    print(c.getC()[:lim] if c else "<fail>")
dec(0x103827a88,"GetSystem call site A (FUN_103827a88)")
dec(0x103fcd420,"GetSystem call site B (FUN_103fcd420)")
dec(0x102ddd354,"GetSystem call site C (FUN_102ddd354)")
