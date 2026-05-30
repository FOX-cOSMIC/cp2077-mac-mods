# -*- coding: utf-8 -*-
from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
di = DecompInterface(); di.openProgram(currentProgram)
def dec(addr, name, timeout=120):
    f = getFunctionContaining(toAddr(addr))
    if not f:
        print("== NO FUNC at 0x%x (%s) =="%(addr,name)); return
    r = di.decompileFunction(f, timeout, ConsoleTaskMonitor())
    print("\n##### %s  (%s @ 0x%x) #####"%(name, f.getName(), addr))
    c = r.getDecompiledFunction()
    print(c.getC() if c else "<decompile failed>")
dec(0x102b73b50, "TweakDB_ctor")
dec(0x1026b8db8, "CreateTDBRecord")
