// h001_probe.cpp — diagnostic probe for H-001
//
// Tests whether macOS TweakDB uses a function-pointer dispatch table.
// Load with DYLD_INSERT_LIBRARIES against the stock 2.3.1 binary (F-001).
// No inline __TEXT hooking, no mprotect, no MAP_JIT. Read-only diagnostic.
//
// Strategy (per H-001 test plan):
//  1. Register a dyld image callback; wait for Cyberpunk2077 main binary.
//  2. Log image base, slide, segment bounds (cross-reference with Ghidra).
//  3. Scan LC_SYMTAB for TweakDB-related names (best-effort — likely stripped).
//  4. Try dlsym() for known exported TweakDB getter names.
//     If a getter is found, CALL it and dump the singleton (expected null
//     at image-load time; non-null is a bonus).
//  5. Regardless of getter result, dump the first 64 qwords of __DATA and
//     __DATA_CONST as a structural probe of what's initialized at load time.
//  6. For every qword whose value falls inside __TEXT, log 16 raw bytes at
//     that address (ARM64 function prologue candidates).
//
// All speculative reads go through mach_vm_read_overwrite() — returns a
// kernel error for unmapped pages instead of crashing.

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>

// ---------------------------------------------------------------------------
// Logging — flush after every write; probe must survive a game crash mid-run.
// ---------------------------------------------------------------------------

static FILE*       g_log = nullptr;
static const char* k_log = "/tmp/h001-probe.log";

static void log_write(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
static void log_write(const char* fmt, ...) {
    if (!g_log) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(g_log, fmt, ap);
    va_end(ap);
    fflush(g_log);
}

static void log_ts(const char* msg) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    log_write("[H001] %lld.%09ld  %s\n", (long long)ts.tv_sec, (long)ts.tv_nsec, msg);
}

// ---------------------------------------------------------------------------
// Safe memory read — KERN_INVALID_ADDRESS for unmapped pages; never crashes.
// ---------------------------------------------------------------------------

static bool safe_read(uint64_t addr, void* buf, size_t size) {
    mach_vm_size_t n  = 0;
    kern_return_t  kr = mach_vm_read_overwrite(
        mach_task_self(),
        (mach_vm_address_t)addr,
        (mach_vm_size_t)size,
        (mach_vm_address_t)buf,
        &n);
    return kr == KERN_SUCCESS && (size_t)n == size;
}

// ---------------------------------------------------------------------------
// Mach-O segment walker
// ---------------------------------------------------------------------------

struct SegInfo {
    uint64_t vmaddr;
    uint64_t vmsize;
    uint64_t fileoff; // only meaningful for __LINKEDIT
};

static void walk_segments(const struct mach_header_64* mh,
                          SegInfo* text, SegInfo* data,
                          SegInfo* data_const, SegInfo* linkedit) {
    *text = *data = *data_const = *linkedit = {};
    const uint8_t* p = (const uint8_t*)(mh + 1);
    for (uint32_t i = 0; i < mh->ncmds; ++i) {
        const struct load_command* lc = (const struct load_command*)p;
        if (lc->cmd == LC_SEGMENT_64) {
            const struct segment_command_64* s = (const struct segment_command_64*)lc;
            if      (strncmp(s->segname, "__TEXT",       16) == 0)
                { text->vmaddr = s->vmaddr; text->vmsize = s->vmsize; }
            else if (strncmp(s->segname, "__DATA",       16) == 0)
                { data->vmaddr = s->vmaddr; data->vmsize = s->vmsize; }
            else if (strncmp(s->segname, "__DATA_CONST", 16) == 0)
                { data_const->vmaddr = s->vmaddr; data_const->vmsize = s->vmsize; }
            else if (strncmp(s->segname, "__LINKEDIT",   16) == 0) {
                linkedit->vmaddr   = s->vmaddr;
                linkedit->vmsize   = s->vmsize;
                linkedit->fileoff  = s->fileoff;
            }
        }
        p += lc->cmdsize;
    }
}

// ---------------------------------------------------------------------------
// Symtab scan — best-effort; stripped commercial binaries will return no hits.
// ---------------------------------------------------------------------------

static void scan_symtab(const struct mach_header_64* mh, intptr_t slide,
                        const SegInfo& linkedit) {
    const struct symtab_command* st = nullptr;
    const uint8_t* p = (const uint8_t*)(mh + 1);
    for (uint32_t i = 0; i < mh->ncmds; ++i) {
        const struct load_command* lc = (const struct load_command*)p;
        if (lc->cmd == LC_SYMTAB) { st = (const struct symtab_command*)lc; break; }
        p += lc->cmdsize;
    }
    if (!st || st->nsyms == 0 || !linkedit.vmaddr) {
        log_write("[H001]   [symtab] not available (stripped or no LINKEDIT)\n");
        return;
    }

    // In-process address of a file offset fo:
    //   (linkedit.vmaddr + slide) + (fo - linkedit.fileoff)
    uint64_t li_base  = linkedit.vmaddr + (uint64_t)slide;
    uint64_t sym_base = li_base + (st->symoff - linkedit.fileoff);
    uint64_t str_base = li_base + (st->stroff - linkedit.fileoff);

    static const char* const needles[] = {
        "TweakDB", "tweakdb", "GetFlat", "SetFlat",
        "flatData", "staticFlat", "TweakDBID", nullptr
    };

    // Cap to avoid blocking for too long on non-stripped builds.
    uint32_t limit = st->nsyms < 20000u ? st->nsyms : 20000u;
    if (st->nsyms > limit)
        log_write("[H001]   [symtab] %u total symbols; scanning first %u\n",
                  st->nsyms, limit);

    uint32_t hits = 0;
    for (uint32_t i = 0; i < limit; ++i) {
        struct nlist_64 sym;
        if (!safe_read(sym_base + i * sizeof(sym), &sym, sizeof(sym))) break;
        if (!sym.n_un.n_strx) continue;

        char name[256] = {};
        if (!safe_read(str_base + sym.n_un.n_strx, name, sizeof(name) - 1)) continue;

        for (int k = 0; needles[k]; ++k) {
            if (strstr(name, needles[k])) {
                log_write("[H001]   [sym] %-72s vmaddr=0x%llx  slid=0x%llx\n",
                          name,
                          (unsigned long long)sym.n_value,
                          (unsigned long long)(sym.n_value + (uint64_t)slide));
                ++hits;
                break;
            }
        }
    }
    if (!hits)
        log_write("[H001]   [symtab] no TweakDB-related symbols in %u scanned entries\n",
                  limit);
}

// ---------------------------------------------------------------------------
// __DATA region dump — first 64 qwords; annotates __TEXT pointers inline.
// ---------------------------------------------------------------------------

static void dump_data_region(const char* seg_name, uint64_t region_slid,
                             uint64_t region_size,
                             uint64_t text_lo, uint64_t text_hi,
                             intptr_t slide) {
    log_write("[H001] --- %s dump (first 64 qwords @ 0x%llx) ---\n",
              seg_name, (unsigned long long)region_slid);

    uint64_t n = region_size / 8;
    if (n > 64) n = 64;

    uint32_t code_ptrs = 0;
    for (uint64_t j = 0; j < n; ++j) {
        uint64_t slot = region_slid + j * 8;
        uint64_t val  = 0;
        if (!safe_read(slot, &val, 8)) {
            log_write("[H001]   [+%04llx] UNREADABLE\n",
                      (unsigned long long)(j * 8));
            continue;
        }

        bool is_code = (val >= text_lo && val < text_hi);
        log_write("[H001]   [+%04llx] 0x%016llx%s",
                  (unsigned long long)(j * 8),
                  (unsigned long long)val,
                  is_code ? "  CODE" : "");

        if (is_code) {
            log_write(" vmaddr=0x%llx",
                      (unsigned long long)(val - (uint64_t)slide));
            uint8_t raw[16] = {};
            if (safe_read(val, raw, sizeof(raw))) {
                log_write(" |");
                for (int b = 0; b < 16; ++b) log_write(" %02x", (unsigned)raw[b]);
                log_write(" |");
            } else {
                log_write(" <bytes unreadable>");
            }
            ++code_ptrs;
        }
        log_write("\n");
    }
    log_write("[H001]   => %u code pointer(s) in %llu qwords\n",
              code_ptrs, (unsigned long long)n);
}

// ---------------------------------------------------------------------------
// TweakDB singleton probe via dlsym
// ---------------------------------------------------------------------------

static void probe_tweakdb_singleton(uint64_t text_lo, uint64_t text_hi,
                                    intptr_t slide) {
    // Mangled names most likely to be exported by the macOS build.
    // (Binary is probably stripped; all will return null — that's a finding too.)
    static const char* const candidates[] = {
        "_ZN6RED4ext7TweakDB3GetEv",           // RED4ext::TweakDB::Get()
        "_ZN7TweakDB3GetEv",                    // TweakDB::Get()
        "_ZN6RED4ext7TweakDB11GetInstanceEv",   // RED4ext::TweakDB::GetInstance()
        "_ZN6RED4ext7TweakDB14GetDefaultInstEv",
        "_ZN8RED4ext7TweakDB3GetEv",            // alternate ns encoding
        nullptr
    };

    bool found_any = false;
    for (int i = 0; candidates[i]; ++i) {
        void* sym = dlsym(RTLD_DEFAULT, candidates[i]);
        if (!sym) {
            log_write("[H001]   dlsym(%-48s) not found\n", candidates[i]);
            continue;
        }
        found_any = true;
        log_write("[H001]   dlsym(%-48s) -> %p  vmaddr=0x%llx  FOUND\n",
                  candidates[i], sym,
                  (unsigned long long)((uint64_t)(uintptr_t)sym - (uint64_t)slide));

        // Call the getter.  Expected null before TweakDB init; log either result.
        typedef void* (*GetFn)(void);
        void* singleton = reinterpret_cast<GetFn>(sym)();
        log_write("[H001]   singleton = %p%s\n", singleton,
                  singleton ? "" : "  (null — TweakDB not yet initialized)");

        if (!singleton) continue;

        // Non-null: dump first 64 qwords from the singleton.
        uint64_t base = (uint64_t)(uintptr_t)singleton;
        log_write("[H001]   --- singleton dump (64 qwords @ 0x%llx) ---\n",
                  (unsigned long long)base);
        for (int q = 0; q < 64; ++q) {
            uint64_t qval = 0;
            if (!safe_read(base + (uint64_t)(q * 8), &qval, 8)) {
                log_write("[H001]     [%02d] UNREADABLE\n", q);
                continue;
            }
            bool in_text = (qval >= text_lo && qval < text_hi);
            log_write("[H001]     [%02d] 0x%016llx%s",
                      q, (unsigned long long)qval,
                      in_text ? "  <TEXT>" : "");
            if (in_text) {
                uint8_t raw[16] = {};
                if (safe_read(qval, raw, 16)) {
                    log_write(" bytes:");
                    for (int b = 0; b < 16; ++b)
                        log_write(" %02x", (unsigned)raw[b]);
                } else {
                    log_write(" <bytes unreadable>");
                }
            }
            log_write("\n");
        }
        return; // One non-null dump is sufficient.
    }

    if (!found_any)
        log_write("[H001]   no TweakDB getter exported — binary likely stripped\n");
}

// ---------------------------------------------------------------------------
// dyld image-load callback
// ---------------------------------------------------------------------------

static void on_image_added(const struct mach_header* mh, intptr_t slide) {
    if (!g_log || mh->magic != MH_MAGIC_64) return;

    // Resolve path by matching mh pointer against the dyld image table.
    const char* path = nullptr;
    for (uint32_t i = 0, n = _dyld_image_count(); i < n; ++i) {
        if (_dyld_get_image_header(i) == mh) {
            path = _dyld_get_image_name(i);
            break;
        }
    }
    // Match main binary only — not injected dylibs.
    if (!path || !strstr(path, "Cyberpunk2077") || strstr(path, ".dylib")) return;

    const struct mach_header_64* mh64 = (const struct mach_header_64*)mh;

    SegInfo text, data, dc, linkedit;
    walk_segments(mh64, &text, &data, &dc, &linkedit);

    uint64_t text_lo = text.vmaddr + (uint64_t)slide;
    uint64_t text_hi = text_lo + text.vmsize;

    log_write("[H001] ========== Cyberpunk2077 binary detected ==========\n");
    log_write("[H001]   path    = %s\n", path);
    log_write("[H001]   base    = 0x%016llx\n", (unsigned long long)(uintptr_t)mh);
    log_write("[H001]   slide   = 0x%016llx\n", (unsigned long long)(uint64_t)slide);
    log_write("[H001]   __TEXT  = [0x%llx, 0x%llx)  size=%llu KB\n",
              (unsigned long long)text_lo, (unsigned long long)text_hi,
              (unsigned long long)((text_hi - text_lo) / 1024));
    if (data.vmsize)
        log_write("[H001]   __DATA  = vmaddr=0x%llx  size=%llu KB\n",
                  (unsigned long long)data.vmaddr,
                  (unsigned long long)(data.vmsize / 1024));
    if (dc.vmsize)
        log_write("[H001]   __DATA_CONST = vmaddr=0x%llx  size=%llu KB\n",
                  (unsigned long long)dc.vmaddr,
                  (unsigned long long)(dc.vmsize / 1024));

    log_write("[H001] --- symtab scan ---\n");
    scan_symtab(mh64, slide, linkedit);

    log_write("[H001] --- singleton getter probe ---\n");
    probe_tweakdb_singleton(text_lo, text_hi, slide);

    // Always dump __DATA / __DATA_CONST: captures compile-time-initialized
    // function-pointer tables even if the runtime singleton is null.
    if (data.vmsize > 0)
        dump_data_region("__DATA",
                         data.vmaddr + (uint64_t)slide, data.vmsize,
                         text_lo, text_hi, slide);

    if (dc.vmsize > 0)
        dump_data_region("__DATA_CONST",
                         dc.vmaddr + (uint64_t)slide, dc.vmsize,
                         text_lo, text_hi, slide);

    log_write("[H001] ========== probe complete ==========\n");
    fflush(g_log);
}

// ---------------------------------------------------------------------------
// Constructor — entry point on dylib load
// ---------------------------------------------------------------------------

__attribute__((constructor))
static void h001_probe_init(void) {
    g_log = fopen(k_log, "w");
    if (!g_log) return;
    log_ts("dylib loaded");
    log_write("[H001] log = %s\n", k_log);
    // Fires synchronously for already-loaded images (including the main binary)
    // then for each future load.
    _dyld_register_func_for_add_image(on_image_added);
    log_write("[H001] dyld image callback registered\n");
    fflush(g_log);
}
