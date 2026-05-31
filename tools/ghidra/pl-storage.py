# -*- coding: utf-8 -*-
from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
di = DecompInterface(); di.openProgram(currentProgram)
def dec(addr,name,t=90):
    f=getFunctionContaining(toAddr(addr))
    if not f: print("no func @%x"%addr); return
    r=di.decompileFunction(f,t,ConsoleTaskMonitor())
    c=r.getDecompiledFunction()
    print("\n##### %s (%s @ 0x%x) #####"%(name,f.getName(),addr))
    print(c.getC() if c else "<fail>")
# who reads the PlayerSystem pool storage global 0x1073d63d0
print("=== readers of PlayerSystem storage 0x1073d63d0 ===")
for ref in getReferencesTo(toAddr(0x1073d63d0)):
    f=getFunctionContaining(ref.getFromAddress())
    print("  %s in %s"%(ref.getFromAddress(), f.getName() if f else "?"))
# decompile GetLocalPlayerMainGameObject to see how it gets PlayerSystem
dec(0x103fd1c18,"GetLocalPlayerMainGameObject")
