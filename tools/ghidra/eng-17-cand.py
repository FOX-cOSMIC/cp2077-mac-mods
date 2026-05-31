# -*- coding: utf-8 -*-
from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
di=DecompInterface(); di.openProgram(currentProgram); mon=ConsoleTaskMonitor()
mem=currentProgram.getMemory()
def blk(a):
    b=mem.getBlock(toAddr(a)); return b.getName() if b else "?"
def xrefs(addr,label):
    print("=== xrefs to %s 0x%x [%s] ==="%(label,addr,blk(addr)))
    for r in getReferencesTo(toAddr(addr)):
        f=getFunctionContaining(r.getFromAddress())
        print("   %s %s %s"%(r.getFromAddress(),r.getReferenceType(),f.getName() if f else "?"))
def dec(addr,name,lim=3000):
    f=getFunctionContaining(toAddr(addr))
    if not f: print("no func @%x"%addr); return
    r=di.decompileFunction(f,90,mon); c=r.getDecompiledFunction()
    print("\n##### %s (%s @0x%x) #####"%(name,f.getName(),f.getEntryPoint().getOffset()))
    print(c.getC()[:lim] if c else "<fail>")
xrefs(0x108bac7a0,"candidate framework global")
