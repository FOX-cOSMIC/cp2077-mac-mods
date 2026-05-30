# -*- coding: utf-8 -*-
# Build vtable -> baseHash table by reading slot +0x118 for each record vtable used
# in the factories, decompiling the GetTweakBaseHash fn, and extracting the constant.
# Also identify the type name via GetNativeType (slot +0x08) -> CName string if possible.
from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
import re
di = DecompInterface(); di.openProgram(currentProgram)
mon = ConsoleTaskMonitor()
mem = currentProgram.getMemory()

def rd64(a): return mem.getLong(toAddr(a)) & 0xffffffffffffffff

def const_of(vtbl):
    """Return the baseHash constant from slot +0x118 of vtbl, or None."""
    p = rd64(vtbl+0x118)
    f = getFunctionContaining(toAddr(p))
    if not f: return (None, p, None)
    r = di.decompileFunction(f, 60, mon)
    c = r.getDecompiledFunction()
    body = c.getC() if c else ""
    m = re.search(r'return (0x[0-9a-fA-F]+|[0-9]+);', body)
    val = None
    if m:
        try: val = int(m.group(1),0)
        except: val=None
    return (val, p, f.getName())

def native_type_str(vtbl):
    """Slot +0x08 GetNativeType: decompile and try to find the CName/type string."""
    p = rd64(vtbl+0x08)
    f = getFunctionContaining(toAddr(p))
    if not f: return ""
    r = di.decompileFunction(f, 60, mon)
    c = r.getDecompiledFunction()
    body = c.getC() if c else ""
    # grab data-string references
    return body.replace("\n"," ")[:200]

# All distinct &PTR_FUN vtables seen so far (from factory_case0/1/2) + key ones.
# (label, static PTR/vtable addr)
vtables = [
  ("type@0x1885c3e0 (.statType/.value)", 0x10704f2e8),
  ("case0 .icons", 0x10704f678),
  ("case0 .appearanceSuffixes", 0x10704e378),
  ("case0 .boundingShapeRelativeArea", 0x10704deb8),
  ("case0 .map", 0x10704dd88),
  ("case0 .bountyChoices", 0x10704dfe8),
  ("case0 .objectList", 0x10704f548),
  ("case0 .target+.abilities", 0x10704d538),
  ("case0 .target(0x382d48a0)", 0x10704db28),
  ("case0 .OR", 0x10704d9f8),
  ("case0 .perk", 0x10704ea98),
  ("case0 .fireDelay", 0x10704f1b8),
  ("case0 0x4bd0bce0", 0x10704e4a8),
  ("case0 0x45cd0820", 0x10704e5d8),
  ("case2 .modifierType(-0x430cadde)", 0x107057400),
  ("case2 .enumName", 0x107058110),
  ("case1 .items", 0x107053fd8),
  ("case1 .abilities", 0x107052cd8),
  ("case1 .pool", 0x107053068),
]
print("%-40s %-12s %-12s %s" % ("label","vtableStatic","baseHash","getHashFn"))
for label,v in vtables:
    val,p,fn = const_of(v)
    vs = ("0x%x"%(val & 0xffffffff)) if val is not None else "NONE"
    print("%-40s 0x%-10x %-12s %s @0x%x" % (label, v, vs, fn or "?", p))

# Identify the gamedataStat_Record by checking GetNativeType decompile of the prime suspect
print("\n--- GetNativeType bodies (type-id hints) ---")
for label,v in [("0x1885c3e0",0x10704f2e8),("0x430cadde-modifier",0x107057400)]:
    print(" %s :: %s" % (label, native_type_str(v)))
