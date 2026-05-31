# -*- coding: utf-8 -*-
from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
di=DecompInterface(); di.openProgram(currentProgram); mon=ConsoleTaskMonitor()
mem=currentProgram.getMemory()
def blk(a):
    b=mem.getBlock(toAddr(a)); return b.getName() if b else "?"
def callers(addr,label,maxn=30):
    print("=== callers of %s 0x%x ==="%(label,addr)); n=0
    for r in getReferencesTo(toAddr(addr)):
        rt=str(r.getReferenceType())
        if "CALL" in rt or "UNCONDITIONAL" in rt:
            f=getFunctionContaining(r.getFromAddress())
            print("   %s %s %s"%(r.getFromAddress(),rt,f.getName() if f else "?")); n+=1
            if n>=maxn: break
def dec(addr,name,lim=2600):
    f=getFunctionContaining(toAddr(addr))
    if not f: print("no func @%x"%addr); return
    r=di.decompileFunction(f,80,mon); c=r.getDecompiledFunction()
    print("\n##### %s (%s @0x%x) #####"%(name,f.getName(),f.getEntryPoint().getOffset()))
    print(c.getC()[:lim] if c else "<fail>")
# The CGameEngine CClass accessors (small getters) - who calls them = engine consumers
dec(0x103f1ff84,"CGameEngine type getter A")
callers(0x103f1ff84,"CGameEngine type getter A")
# BaseGameEngine type accessor
callers(0x1035f1914,"BaseGameEngine type-hash fn")
