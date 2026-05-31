# -*- coding: utf-8 -*-
# Look for a global-rooted framework: ldr R,[Gslot] (global), then ldr _,[R,#0x10].
# AND separately: ldr R,[Gslot], then ldr _,[R,#0x308]. Track callee-saved across whole fn.
fm=currentProgram.getFunctionManager()
listing=currentProgram.getListing()
mem=currentProgram.getMemory()
DATABLOCKS=("__data","__bss","__common","__got","__const")
def blk(a):
    b=mem.getBlock(a); return b.getName() if b else "?"
res10=[]; res308=[]
for f in fm.getFunctions(True):
    regsrc={}
    for i in listing.getInstructions(f.getBody(),True):
        mn=i.getMnemonicString(); rd=i.getRegister(0); txt=i.toString()
        if mn=="ldr":
            base=i.getRegister(1)
            if base is not None and base.toString() in regsrc:
                if "#0x10]" in txt:
                    res10.append((f.getEntryPoint().getOffset(),f.getName(),regsrc[base.toString()],i.getAddress().getOffset()))
                if "#0x308]" in txt:
                    res308.append((f.getEntryPoint().getOffset(),f.getName(),regsrc[base.toString()],i.getAddress().getOffset()))
            gref=None
            for r in i.getReferencesFrom():
                ta=r.getToAddress()
                if ta is not None and ta.isMemoryAddress() and blk(ta) in DATABLOCKS:
                    gref=ta.getOffset()
            if rd is not None:
                if gref is not None: regsrc[rd.toString()]=gref
                elif rd.toString() in regsrc: del regsrc[rd.toString()]
        else:
            if rd is not None and rd.toString() in regsrc: del regsrc[rd.toString()]
from collections import defaultdict
print("=== global -> +0x10 (framework->gameInstance candidates): %d sites ==="%len(res10))
g10=defaultdict(int)
for ent,nm,g,site in res10: g10[g]+=1
# Only show globals that ALSO appear in res308 (framework: read at both +0x10 AND +0x308)
g308=set(g for ent,nm,g,site in res308)
print("globals with BOTH +0x10 and +0x308 reads (framework signature):")
for g in sorted(set(g10.keys()) & g308):
    print("   GLOBAL 0x%x [%s]  (+0x10 reads:%d)"%(g,blk(toAddr(g)),g10[g]))
print("\ntop globals by +0x10 read count:")
for g in sorted(g10.keys(),key=lambda k:-g10[k])[:25]:
    print("   0x%x [%s] x%d"%(g,blk(toAddr(g)),g10[g]))
