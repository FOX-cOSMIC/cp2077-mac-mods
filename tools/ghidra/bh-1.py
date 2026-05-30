# -*- coding: utf-8 -*-
from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
di = DecompInterface(); di.openProgram(currentProgram)
mon = ConsoleTaskMonitor()
mem = currentProgram.getMemory()

def dec(addr, name, timeout=180):
    f = getFunctionContaining(toAddr(addr))
    if not f:
        print("== NO FUNC at 0x%x (%s) =="%(addr,name)); return None
    r = di.decompileFunction(f, timeout, mon)
    print("\n##### %s  (%s @ entry 0x%x) #####"%(name, f.getName(), f.getEntryPoint().getOffset()))
    c = r.getDecompiledFunction()
    print(c.getC() if c else "<decompile failed>")
    return c.getC() if c else None

# 1) CreateTDBRecord - to confirm switch and get the 0x1f tail factory
dec(0x1026b8db8, "CreateTDBRecord")

# 2) Priority factory: gamedataStat_Record lives in one of these.
# Decompile a few factories; we'll grep their output for vtable constant assignments.
for a,n in [(0x1027052ac,"factory_case0"),
            (0x102726504,"factory_case1"),
            (0x10274f578,"factory_case2"),
            (0x102798288,"factory_case4")]:
    dec(a,n)
