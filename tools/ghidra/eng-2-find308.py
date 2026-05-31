# -*- coding: utf-8 -*-
# Find the engine static global: locate functions whose code reads *(GLOBAL)
# then +0x308 then +0x10. Strategy: scan instructions for the constant 0x308
# used as a displacement load, near a preceding load from a __data/__bss global.
from ghidra.program.model.address import AddressSet
fm = currentProgram.getFunctionManager()
listing = currentProgram.getListing()

mem = currentProgram.getMemory()
def blockname(a):
    b = mem.getBlock(a)
    return b.getName() if b else "?"

# Pass 1: find all instructions that add/load with displacement 0x308.
# Then within the same function look for a reference to a data global (in __data/__bss/__const)
# that is loaded right before. We collect candidate functions.
hits = {}
ins = listing.getInstructions(True)
count=0
for i in ins:
    count+=1
    txt = i.toString()
    if "0x308" in txt:
        f = fm.getFunctionContaining(i.getAddress())
        fn = f.getName() if f else "?"
        ent = f.getEntryPoint().getOffset() if f else 0
        hits.setdefault(ent,(fn,[]))
        hits[ent][1].append((i.getAddress().getOffset(), txt))
print("scanned %d instructions; %d functions touch 0x308" % (count,len(hits)))
# Print first 40 candidate functions with their 0x308 sites
shown=0
for ent in sorted(hits.keys()):
    fn,sites=hits[ent]
    print("FUN %s @ 0x%x  (%d sites)" % (fn,ent,len(sites)))
    for off,txt in sites[:3]:
        print("    0x%x: %s" % (off,txt))
    shown+=1
    if shown>=60:
        print("... (%d more)"%(len(hits)-shown)); break
