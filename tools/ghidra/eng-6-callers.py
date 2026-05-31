# -*- coding: utf-8 -*-
from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
di=DecompInterface(); di.openProgram(currentProgram); mon=ConsoleTaskMonitor()
def callers(addr,label):
    print("=== callers of %s 0x%x ==="%(label,addr))
    for r in getReferencesTo(toAddr(addr)):
        if str(r.getReferenceType()).find("CALL")>=0 or str(r.getReferenceType()).find("UNCONDITIONAL")>=0:
            f=getFunctionContaining(r.getFromAddress())
            print("   %s  %s  %s"%(r.getFromAddress(),r.getReferenceType(),f.getName() if f else "?"))
def dec(addr,name,lim=4500):
    f=getFunctionContaining(toAddr(addr))
    if not f: print("no func @%x"%addr); return
    r=di.decompileFunction(f,150,mon); c=r.getDecompiledFunction()
    print("\n########## %s (%s @0x%x) ##########"%(name,f.getName(),f.getEntryPoint().getOffset()))
    print(c.getC()[:lim] if c else "<fail>")
callers(0x1039a6454,"readerA")
callers(0x1039a6460,"readerB")
callers(0x1039a646c,"readerC")
# CGameEngine type hash function
dec(0x103f22e38,"CGameEngine type-hash fn")
