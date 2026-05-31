# -*- coding: utf-8 -*-
from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
di=DecompInterface(); di.openProgram(currentProgram); mon=ConsoleTaskMonitor()
def dec(addr,name,lim=4500):
    f=getFunctionContaining(toAddr(addr))
    if not f: print("no func @%x"%addr); return
    r=di.decompileFunction(f,150,mon); c=r.getDecompiledFunction()
    print("\n########## %s (%s @0x%x) ##########"%(name,f.getName(),f.getEntryPoint().getOffset()))
    print(c.getC()[:lim] if c else "<fail>")
dec(0x1039a7384,"PlayerSystemType populator (writes DAT_1090d9090)")
dec(0x1039a6454,"reader-A of DAT_1090d9090")
dec(0x1039a6460,"reader-B of DAT_1090d9090")
dec(0x1039a646c,"reader-C of DAT_1090d9090")
dec(0x1039a7480,"reader-D of DAT_1090d9090")
