# -*- coding: utf-8 -*-
from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
di=DecompInterface(); di.openProgram(currentProgram); mon=ConsoleTaskMonitor()
mem=currentProgram.getMemory()
def blk(a):
    b=mem.getBlock(toAddr(a)); return b.getName() if b else "?"
def xrefs(addr,label,maxn=40):
    print("=== xrefs to %s 0x%x [%s] ==="%(label,addr,blk(addr)))
    n=0
    for r in getReferencesTo(toAddr(addr)):
        fa=r.getFromAddress(); f=getFunctionContaining(fa)
        print("   %s %s in %s"%(fa,r.getReferenceType(),f.getName() if f else "?")); n+=1
        if n>=maxn: break
def dec(addr,name,lim=3500):
    f=getFunctionContaining(toAddr(addr))
    if not f: print("no func @%x"%addr); return
    r=di.decompileFunction(f,150,mon); c=r.getDecompiledFunction()
    print("\n##### %s (%s @0x%x) #####"%(name,f.getName(),f.getEntryPoint().getOffset()))
    print(c.getC()[:lim] if c else "<fail>")
# CGameEngine CClass instance & cached type
xrefs(0x1090ddb18,"DAT_1090ddb18 (CGameEngine CClass cache)")
xrefs(0x10897f190,"DAT_10897f190 (CGameEngine CClass storage)")
