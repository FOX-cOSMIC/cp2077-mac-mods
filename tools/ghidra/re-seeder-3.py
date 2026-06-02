# -*- coding: utf-8 -*-
# Phase A1 (cont): array accessor layout + ConstantStatModifier value offset.
from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

di = DecompInterface(); di.openProgram(currentProgram)
mon = ConsoleTaskMonitor()

def decomp(addr, lo=0, hi=4000):
    fn = getFunctionContaining(toAddr(addr))
    if not fn: return ("NOFN", None, None)
    r = di.decompileFunction(fn, 180, mon)
    if r and r.getDecompiledFunction():
        return (fn.getName(), r.getDecompiledFunction().getC()[lo:hi], fn)
    return (fn.getName(), None, fn)

# array accessor used as FUN_100a261ac(record+0x54) -> {data@0, cap@8, count@0xc}
for a in [0x100a261ac, 0x103ab81e8, 0x103a85dc8]:
    nm,c,fn = decomp(a, hi=2500)
    print("="*78); print("%s = %s" % (hex(a), nm)); print("-"*78)
    print(c)

# DAT_1090cd850 is the modifier-type ptr the gather compares against.
# Print the symbol / data at that address and any nearby type-name string.
print("="*78); print("DAT_1090cd850 + type identity"); print("-"*78)
addr = toAddr(0x1090cd850)
d = getDataAt(addr)
print("data@1090cd850:", d)
# follow the pointer
try:
    ptr = getLong(addr) & 0xffffffffffffffff
    print("  -> 0x%x" % ptr)
    p2 = toAddr(ptr)
    # RTTI type objects often have a name ptr early; dump a few qwords
    for off in range(0, 0x40, 8):
        v = getLong(toAddr(ptr+off)) & 0xffffffffffffffff
        s = ""
        try:
            da = getDataAt(toAddr(v))
            if da is not None: s = str(da)
        except: pass
        print("   +0x%02x: 0x%x %s" % (off, v, s))
except Exception as e:
    print("  err", e)
