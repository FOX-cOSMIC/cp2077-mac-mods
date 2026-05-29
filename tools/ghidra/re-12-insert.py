from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
di = DecompInterface(); di.openProgram(currentProgram)
mon = ConsoleTaskMonitor()
def decomp(addr):
    fn = getFunctionContaining(toAddr(addr))
    if not fn: return ("NOFN",None,None)
    r = di.decompileFunction(fn, 120, mon)
    if r and r.getDecompiledFunction(): return (fn.getName(), r.getDecompiledFunction().getC(), fn)
    return (fn.getName(),None,fn)

# FUN_103a7d4bc likely recompute/set; FUN_103a8541c / FUN_103a854b4 set value into container.
# Find the one that READS a Stat_Record (+0x54) and writes container entry+0xc.
for a in [0x103a7d4bc, 0x103a8541c, 0x103a854b4, 0x103a7b770]:
    nm,c,fn=decomp(a)
    print("="*72); print("CAND %s = %s" % (hex(a),nm)); print("-"*72)
    if c:
        print(c[:3000])
        print("   has +0x54:", "+ 0x54)" in c, " GetFlat:", "FUN_102b76708" in c, " getter:", "FUN_102b73c7c" in c, " entry+0xc write:", "+ 0xc)" in c)
