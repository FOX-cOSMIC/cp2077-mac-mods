# -*- coding: utf-8 -*-
# Identify the type NAME for the 0x1885c3e0 vtable (0x10704f2e8).
# Strategy: GetNativeType returns record[6] (a cached CClass*). The CClass is built
# once and registered. Find references to the vtable address and look for the type
# registrar / name string. Also: search the whole image for the constant 0x1885c3e0
# to find the murmur3-registration site that ties name->hash.
from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
di = DecompInterface(); di.openProgram(currentProgram)
mon = ConsoleTaskMonitor()
mem = currentProgram.getMemory()
st = currentProgram.getSymbolTable()

def refs_to(addr):
    out=[]
    for r in getReferencesTo(toAddr(addr)):
        out.append((r.getFromAddress(), r.getReferenceType().toString()))
    return out

print("=== refs to Stat vtable 0x10704f2e8 ===")
for fa,rt in refs_to(0x10704f2e8):
    f = getFunctionContaining(fa)
    print("  from 0x%x (%s) [%s]" % (fa.getOffset(), f.getName() if f else "?", rt))

# Decompile the ctor used right before *rec=&PTR (FUN_102b73ac0 is generic base).
# The actual type-specific ctor for Stat: none shown; vtable set directly. So the type
# name string is registered elsewhere. Search for string "Stat" classref near the
# GetNativeType cache-fill. The cache-fill happens in a function that does
# record[6] = SomeCClass; let's decompile GetNativeType fully + its jumptable target.
def decfull(a,label):
    f=getFunctionContaining(toAddr(a))
    if not f:
        print("no fn @0x%x"%a); return
    r=di.decompileFunction(f,120,mon); c=r.getDecompiledFunction()
    print("\n--- %s : %s @0x%x ---"%(label,f.getName(),f.getEntryPoint().getOffset()))
    print(c.getC() if c else "<fail>")

decfull(0x1026c9d80, "Stat GetNativeType (slot+0x08)")
