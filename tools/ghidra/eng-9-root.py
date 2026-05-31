# -*- coding: utf-8 -*-
# Whole-function tracking: any global G such that within ONE function there is
# ldr R,[Gslot] and later ldr _,[R,#0x308]. Track reg-from-global across the
# whole function (kill on redefinition). Report (function, global).
fm=currentProgram.getFunctionManager()
listing=currentProgram.getListing()
mem=currentProgram.getMemory()
def blk(a):
    b=mem.getBlock(a); return b.getName() if b else "?"
DATABLOCKS=("__data","__bss","__common","__got","__la_symbol_ptr","__const","__cfstring")
results=[]
funcs=fm.getFunctions(True)
nf=0
for f in funcs:
    nf+=1
    body=f.getBody()
    it=listing.getInstructions(body,True)
    regsrc={}   # reg -> global addr (loaded from that global slot)
    for i in it:
        mn=i.getMnemonicString()
        rd=i.getRegister(0)
        if mn=="ldr":
            txt=i.toString()
            # is it ldr X,[base,#0x308]?
            base=i.getRegister(1)
            if "#0x308]" in txt and base is not None and base.toString() in regsrc:
                results.append((f.getName(),f.getEntryPoint().getOffset(),regsrc[base.toString()],i.getAddress().getOffset()))
            # does this ldr load from a data global?
            gref=None
            for r in i.getReferencesFrom():
                ta=r.getToAddress()
                if ta is not None and ta.isMemoryAddress() and blk(ta) in DATABLOCKS:
                    gref=ta.getOffset()
            if rd is not None:
                if gref is not None: regsrc[rd.toString()]=gref
                elif rd.toString() in regsrc: del regsrc[rd.toString()]
        else:
            # any write to rd kills tracking for it
            if rd is not None and rd.toString() in regsrc:
                del regsrc[rd.toString()]
print("scanned %d functions; %d (global -> +0x308) hits"%(nf,len(results)))
# dedupe by global
from collections import defaultdict
byg=defaultdict(list)
for fn,ent,g,site in results:
    byg[g].append((fn,ent,site))
for g in sorted(byg.keys()):
    lst=byg[g]
    print("GLOBAL 0x%x [%s] : %d sites; e.g. %s @0x%x (use@0x%x)"%(g,blk(toAddr(g)),len(lst),lst[0][0],lst[0][1],lst[0][2]))
