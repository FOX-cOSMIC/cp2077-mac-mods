# -*- coding: utf-8 -*-
from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
di=DecompInterface(); di.openProgram(currentProgram); mon=ConsoleTaskMonitor()
def dec(addr,name,lim=5000):
    f=getFunctionContaining(toAddr(addr))
    if not f: print("no func @%x"%addr); return
    r=di.decompileFunction(f,120,mon); c=r.getDecompiledFunction()
    print("\n########## %s (%s @0x%x) ##########"%(name,f.getName(),f.getEntryPoint().getOffset()))
    print(c.getC()[:lim] if c else "<fail>")
# FUN_103d09048 had: *plVar5+0x10 (GetSystem) twice then +0x308 call - a system-heavy fn
dec(0x103d09048,"FUN_103d09048 (multi GetSystem)")
dec(0x103c7515c,"FUN_103c7515c")
dec(0x10039ea80,"FUN_10039ea80")
