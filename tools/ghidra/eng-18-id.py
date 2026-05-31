# -*- coding: utf-8 -*-
from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
di=DecompInterface(); di.openProgram(currentProgram); mon=ConsoleTaskMonitor()
def dec(addr,name,lim=3500):
    f=getFunctionContaining(toAddr(addr))
    if not f: print("no func @%x"%addr); return
    r=di.decompileFunction(f,90,mon); c=r.getDecompiledFunction()
    print("\n##### %s (%s @0x%x) #####"%(name,f.getName(),f.getEntryPoint().getOffset()))
    print(c.getC()[:lim] if c else "<fail>")
dec(0x104057610,"writer FUN_104057610")
dec(0x10405a6fc,"writer2 FUN_10405a6fc")
dec(0x10405a68c,"reader FUN_10405a68c (the +0x10 one)")
