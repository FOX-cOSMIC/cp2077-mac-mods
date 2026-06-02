# -*- coding: utf-8 -*-
# Phase A1: trace the Health/Memory stat seeder to the source modifier.
# Decompiles the modifier-gather + fold functions and the recompute, and
# enumerates which functions they call (looking for GetRecord/GetFlat and
# the modifier-array walk) plus their callers (init vs per-query).
from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor

di = DecompInterface(); di.openProgram(currentProgram)
mon = ConsoleTaskMonitor()

GETRECORD = 0x102b745d0
GETFLAT   = 0x102b76708

def fn_at(addr):
    return getFunctionContaining(toAddr(addr))

def decomp(addr, nchars=6000):
    fn = fn_at(addr)
    if not fn: return ("NOFN", None, None)
    r = di.decompileFunction(fn, 180, mon)
    if r and r.getDecompiledFunction():
        return (fn.getName(), r.getDecompiledFunction().getC(), fn)
    return (fn.getName(), None, fn)

def callees(fn):
    out = {}
    body = fn.getBody()
    listing = currentProgram.getListing()
    for ins in listing.getInstructions(body, True):
        for ref in ins.getReferencesFrom():
            if ref.getReferenceType().isCall() or ref.getReferenceType().isJump():
                tgt = ref.getToAddress()
                cf = getFunctionContaining(tgt)
                if cf:
                    out[cf.getEntryPoint().toString()] = cf.getName()
    return out

def callers(fn):
    out = {}
    for ref in getReferencesTo(fn.getEntryPoint()):
        cf = getFunctionContaining(ref.getFromAddress())
        if cf:
            out[cf.getEntryPoint().toString()] = cf.getName()
    return out

TARGETS = [
    ("modifier-gather", 0x103a99a60),
    ("gather-b",        0x103a9b03c),
    ("gather-c",        0x103a9b73c),
    ("recompute-fold",  0x103a7d4bc),
]

for label, addr in TARGETS:
    nm, c, fn = decomp(addr)
    print("="*78)
    print("%s  %s = %s" % (label, hex(addr), nm))
    print("-"*78)
    if c:
        print(c[:6000])
    if fn:
        cs = callees(fn)
        # flag the interesting callees
        flags = []
        for k, v in cs.items():
            if int(k, 16) == GETRECORD: flags.append("CALLS GetRecord @"+k)
            if int(k, 16) == GETFLAT:   flags.append("CALLS GetFlat @"+k)
        print("  -- callees:", len(cs))
        for f in flags: print("    >>", f)
        cl = callers(fn)
        print("  -- callers:", len(cl), [ "%s:%s"%(k,v) for k,v in sorted(cl.items())][:12])
