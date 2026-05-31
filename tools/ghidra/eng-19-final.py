# -*- coding: utf-8 -*-
from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
di=DecompInterface(); di.openProgram(currentProgram); mon=ConsoleTaskMonitor()
def dec(addr,name,lim=2200):
    f=getFunctionContaining(toAddr(addr))
    if not f: print("no func @%x"%addr); return
    r=di.decompileFunction(f,90,mon); c=r.getDecompiledFunction()
    print("\n##### %s (%s @0x%x) #####"%(name,f.getName(),f.getEntryPoint().getOffset()))
    print(c.getC()[:lim] if c else "<fail>")
# FUN_1021864f8 = the Handle wrapper used after GetSystem returns a raw ptr
dec(0x1021864f8,"FUN_1021864f8 (Handle-from-rawptr wrapper)")
# FUN_1039a6db4 = PlayerSystem -> player game object
dec(0x1039a6db4,"FUN_1039a6db4 (PlayerSystem->localPlayer)")
# Confirm what 0x1090d9090 holds: read the pointer value via memory
mem=currentProgram.getMemory()
from ghidra.program.model.address import Address
def rd64(a):
    return mem.getLong(toAddr(a)) & 0xffffffffffffffff
print("\n*(0x1090d9090) raw = 0x%x  (expect 0x108853fa0)"%(rd64(0x1090d9090)))
print("*(0x108853fa0) [vtable of CClass] = 0x%x"%(rd64(0x108853fa0)))
