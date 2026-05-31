# -*- coding: utf-8 -*-
# Find: ldr Xd,[Xs,#0x308] where Xs traces back (within a few instrs) to an
# adrp+ldr from a static global in __data/__bss. Report the global address.
from ghidra.program.model.address import AddressSet
fm = currentProgram.getFunctionManager()
listing = currentProgram.getListing()
mem = currentProgram.getMemory()

def blk(a):
    b = mem.getBlock(a); return b.getName() if b else "?"

# Build map: for each function, walk instructions, track register<-global from adrp/ldr,
# and when we see ldr X,[reg,#0x308], check if reg was loaded from a global.
import re
ins = listing.getInstructions(True)
# We process per function to keep register state local-ish (linear approx).
cur_fn=None
regsrc={}   # reg -> ('global', addr) or ('deref', basereg, disp)
results=[]
def parse(i):
    return i.getMnemonicString(), [str(o) for o in range(i.getNumOperands())]

prev_adrp = {}  # reg -> page address from adrp
count=0
for i in ins:
    count+=1
    f = fm.getFunctionContaining(i.getAddress())
    if f is None:
        continue
    ent=f.getEntryPoint().getOffset()
    if ent!=cur_fn:
        cur_fn=ent; regsrc={}; prev_adrp={}
    mn=i.getMnemonicString()
    txt=i.toString()
    # track adrp Xd, 0xPAGE
    if mn=="adrp":
        rd=i.getRegister(0)
        # operand 1 is a scalar address
        try:
            v=i.getOpObjects(1)[0]
            page=v.getValue() if hasattr(v,'getValue') else long(str(v),16)
        except:
            page=None
        if rd is not None and page is not None:
            prev_adrp[rd.toString()]=page
    elif mn=="ldr":
        rd=i.getRegister(0)
        # ldr Xd,[Xn,#disp]  -> resolve
        refs=i.getReferencesFrom()
        # If this ldr loads from a global (Ghidra resolved an addr ref), mark regsrc[rd]=that addr
        gref=None
        for r in refs:
            ta=r.getToAddress()
            if ta is not None and ta.isMemoryAddress():
                b=mem.getBlock(ta)
                if b and b.getName() in ("__data","__bss","__common","__got","__la_symbol_ptr"):
                    gref=ta.getOffset()
        if rd is not None:
            if gref is not None:
                regsrc[rd.toString()]=('global',gref)
            else:
                # check for [Xn,#0x308]
                if "#0x308]" in txt:
                    # base register
                    base=i.getRegister(1)
                    if base is not None and base.toString() in regsrc and regsrc[base.toString()][0]=='global':
                        results.append((i.getAddress().getOffset(), f.getName(), ent, regsrc[base.toString()][1], txt))
                regsrc[rd.toString()]=('other',None)
        # invalidate
    elif mn in ("add","str","mov","movk","movz"):
        rd=i.getRegister(0)
        if rd is not None and rd.toString() in regsrc:
            del regsrc[rd.toString()]
print("scanned %d instrs; %d chain hits (global -> +0x308)" % (count,len(results)))
for off,fn,ent,g,txt in results:
    print("HIT @0x%x in %s(0x%x): base from global 0x%x [%s] | %s"%(off,fn,ent,g,blk(toAddr(g)),txt))
