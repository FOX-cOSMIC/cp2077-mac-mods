# rw-12: correlate type-hash -> vtable in FUN_102b1422c by parsing the decompiled C,
# and find unk160 writers by scanning DB-region functions for a store to [db+0x160].
from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
import re
di = DecompInterface(); di.openProgram(currentProgram)
mon = ConsoleTaskMonitor()
def fn_at(a): return getFunctionContaining(toAddr(a))

r = di.decompileFunction(fn_at(0x102b1422c), 300, mon)
c = r.getDecompiledFunction().getC()
# Find each "if (lVar16 == <hash>)" ... "*puVarNN = &PTR..." pairing.
lines = c.splitlines()
curhash=None
print("TYPE-HASH -> FlatValue VTABLE  (and data store offset):")
for i,ln in enumerate(lines):
    m = re.search(r'lVar16 == (-?0x[0-9a-fA-F]+)', ln)
    if m: curhash = m.group(1)
    m2 = re.search(r'\*puVar\d+ = (&PTR_\w+);', ln)
    if m2 and curhash:
        # find the data store on a following line
        store=""
        for j in range(i,min(i+4,len(lines))):
            ms=re.search(r'puVar\d+\[(\d+)\] = |\*\(undefined4 \*\)\(puVar\d+ \+ (\d+)\)', lines[j])
            if 'puVar' in lines[j] and '=' in lines[j] and ('[1]' in lines[j] or '+ 1)' in lines[j]):
                store=lines[j].strip()
                break
        print("  hash=%-22s vtable=%s   store: %s" % (curhash, m2.group(1), store))

# unk160 scan: list functions in DB TU that store to offset 0x160 of a long* param.
print("\n--- searching DB functions for db+0x160 access ---")
fm = currentProgram.getFunctionManager()
hits=[]
for a in [0x102b73b50,0x102b73db8,0x102b745d0,0x102b76708,0x102b75744,0x102b16a48,0x102b15ec0,0x1026b8db8,0x102b74408]:
    fn=fn_at(a)
    rr=di.decompileFunction(fn,150,mon)
    if rr and rr.getDecompiledFunction():
        cc=rr.getDecompiledFunction().getC()
        for ln in cc.splitlines():
            if '0x160' in ln:
                hits.append((fn.getName(), ln.strip()))
for h in hits: print("  ",h[0],"::",h[1])
print("  (no db+0x160 hits found in these)" if not hits else "")
