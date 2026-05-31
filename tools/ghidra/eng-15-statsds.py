# -*- coding: utf-8 -*-
from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
di=DecompInterface(); di.openProgram(currentProgram); mon=ConsoleTaskMonitor()
mem=currentProgram.getMemory()
def blk(a):
    b=mem.getBlock(toAddr(a)); return b.getName() if b else "?"
def xrefs(addr,label,maxn=40):
    print("=== xrefs to %s 0x%x [%s] ==="%(label,addr,blk(addr))); n=0
    for r in getReferencesTo(toAddr(addr)):
        f=getFunctionContaining(r.getFromAddress())
        print("   %s %s %s"%(r.getFromAddress(),r.getReferenceType(),f.getName() if f else "?")); n+=1
        if n>=maxn: break
xrefs(0x1090d96a8,"StatsDataSystem type cache DAT_1090d96a8")
# Who reads it (the getter), then who calls that getter (real GetSystem sites with gameInstance)
