# -*- coding: utf-8 -*-
# Reconcile: F-026 says gamedataStat_Record static vtable = 0x107d4a198,
# but the factory writes 0x10704f2e8. Check +0x118 of BOTH; also dump first slots.
# Plus: decompile case 0x1f factory FUN_102afe918 head, and confirm +0x110 stub is shared.
from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
import re
di = DecompInterface(); di.openProgram(currentProgram)
mon = ConsoleTaskMonitor()
mem = currentProgram.getMemory()
def rd64(a):
    try: return mem.getLong(toAddr(a)) & 0xffffffffffffffff
    except: return None

def hashfn(vtbl, off):
    p = rd64(vtbl+off)
    if p is None: return (None,None,None)
    f = getFunctionContaining(toAddr(p))
    if not f: return (None,p,None)
    r=di.decompileFunction(f,60,mon); c=r.getDecompiledFunction()
    body=c.getC() if c else ""
    m=re.search(r'return (0x[0-9a-fA-F]+|[0-9]+);',body)
    v=int(m.group(1),0) if m else None
    return (v,p,f.getName())

for label,v in [("factory-vtable 0x10704f2e8",0x10704f2e8),
                ("F-026 named 0x107d4a198",0x107d4a198)]:
    print("=== %s ==="%label)
    for off in (0x08,0x110,0x118):
        val,p,fn=hashfn(v,off)
        vs=("0x%x"%(val&0xffffffff)) if val is not None else "(not const)"
        print("   +0x%03x -> 0x%x %s  ret=%s"%(off,p or 0,fn or "?",vs))
    print("   slot0(dtor)=0x%x  slot1=0x%x"%(rd64(v) or 0, rd64(v+8) or 0))

# case 0x1f factory head
f=getFunctionContaining(toAddr(0x102afe918))
print("\n=== case 0x1f factory FUN_102afe918 (head) ===")
r=di.decompileFunction(f,120,mon); c=r.getDecompiledFunction()
lines=(c.getC() if c else "").splitlines()
for ln in lines[:40]:
    if ln.strip(): print("   "+ln.strip())
