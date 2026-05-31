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
        fa=r.getFromAddress(); f=getFunctionContaining(fa)
        print("   %s  %s  in %s"%(fa, r.getReferenceType(), f.getName() if f else "?"))

# PlayerSystem type-hash global + cached type object
xrefs(0x107409888,"PlayerSystemTypeHash")
xrefs(0x1090d9090,"DAT_1090d9090 (PlayerSystem type cache)")
# Engine native type-hash guards
xrefs(0x1074035c0,"GetNativeTypeHash<BaseGameEngine>.guard")
xrefs(0x1074035c8,"GetNativeTypeHash<BaseGameEngine>.value")
xrefs(0x1074a31d0,"GetNativeTypeHash<CGameEngine>.guard")
xrefs(0x1073d5700,"GetNativeTypeHash<ScriptGameInstance>.guard")
xrefs(0x1073d7d70,"ScriptGameInstance rttiType")
