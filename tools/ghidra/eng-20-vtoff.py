# -*- coding: utf-8 -*-
# Disassemble the exact GetSystem call sites to confirm the vtable displacement.
listing=currentProgram.getListing()
def dis(start,end,label):
    print("=== %s : 0x%x..0x%x ==="%(label,start,end))
    a=toAddr(start)
    while a.getOffset()<end:
        ins=listing.getInstructionAt(a)
        if ins is None:
            a=a.add(4); continue
        print("  0x%x: %s"%(a.getOffset(),ins.toString()))
        a=ins.getMaxAddress().add(1)
# site C helper FUN_101d97f30 (tiny): *(*param)->vt[0x80]() then vt[0x10]
dis(0x101d97f30,0x101d97f80,"FUN_101d97f30 GetGameInstance(vt0x80)+GetSystem(vt?)")
# FUN_103d09048 first GetSystem call region
dis(0x103d09060,0x103d090a0,"FUN_103d09048 first GetSystem")
