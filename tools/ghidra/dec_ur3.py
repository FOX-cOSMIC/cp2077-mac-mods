# -*- coding: utf-8 -*-
from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
di = DecompInterface(); di.openProgram(currentProgram)
def dec(addr, name, timeout=160):
    f = getFunctionContaining(toAddr(addr))
    if not f: print("== NO FUNC 0x%x =="%addr); return
    r = di.decompileFunction(f, timeout, ConsoleTaskMonitor())
    print("\n##### %s (%s @ 0x%x) #####"%(name, f.getName(), addr))
    c = r.getDecompiledFunction(); print(c.getC() if c else "<fail>")
dec(0x102b73db8, "TweakDB_struct_init")
dec(0x102b16a48, "RecordBuildLoop")
