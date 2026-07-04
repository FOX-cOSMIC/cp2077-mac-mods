// h004_probe.cpp — diagnostic probe for H-004 / T-005 (docs/HYPOTHESES.md, docs/reference/macos-codesigning.md)
//
// Tests whether inline __TEXT hooking (mprotect/vm_protect making a __TEXT
// page RWX) is possible on the CURRENT game build, under a GIVEN re-sign
// entitlement configuration. FA-001's original EPERM finding is from an old
// session on a different project/build and was never re-verified here — this
// probe closes that gap.
//
// SAFETY: this probe NEVER writes a single byte to __TEXT. It only attempts
// to change page protection, and if that succeeds, immediately reverts it
// back to R-X before doing anything else. No hook is installed; no game
// behavior is altered even in the success case. Read-only diagnostic only,
// same discipline as h001_probe.cpp.
//
// Load with DYLD_INSERT_LIBRARIES against a re-signed copy of the game
// binary. See tools/run-h004-probe.sh.

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <cerrno>
#include <unistd.h>
#include <sys/mman.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>

// ---------------------------------------------------------------------------
// Logging — flush after every write; probe must survive a game crash mid-run.
// ---------------------------------------------------------------------------

static FILE*       g_log = nullptr;
static const char* k_log = "/tmp/h004-probe.log";

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
    log_write("[H004] %lld.%09ld  %s\n", (long long)ts.tv_sec, (long)ts.tv_nsec, msg);
}

// ---------------------------------------------------------------------------
// Mach-O segment walker (same approach as h001_probe.cpp)
// ---------------------------------------------------------------------------

struct SegInfo {
    uint64_t vmaddr;
    uint64_t vmsize;
};

static void find_text_segment(const struct mach_header_64* mh, SegInfo* text) {
    *text = {};
    const uint8_t* p = (const uint8_t*)(mh + 1);
    for (uint32_t i = 0; i < mh->ncmds; ++i) {
        const struct load_command* lc = (const struct load_command*)p;
        if (lc->cmd == LC_SEGMENT_64) {
            const struct segment_command_64* s = (const struct segment_command_64*)lc;
            if (strncmp(s->segname, "__TEXT", 16) == 0) {
                text->vmaddr = s->vmaddr;
                text->vmsize = s->vmsize;
                return;
            }
        }
        p += lc->cmdsize;
    }
}

// ---------------------------------------------------------------------------
// The actual H-004 test
// ---------------------------------------------------------------------------

static long page_size() {
    long ps = sysconf(_SC_PAGESIZE);
    return ps > 0 ? ps : 16384; // ARM64 default
}

// Attempts POSIX mprotect() on a __TEXT page. Reverts immediately on success.
// Never writes any bytes to the page regardless of outcome.
static void test_mprotect(uint64_t page_addr, size_t sz) {
    log_write("[H004] --- mprotect() test @ 0x%llx (size=%zu) ---\n",
              (unsigned long long)page_addr, sz);

    int rc = mprotect((void*)page_addr, sz, PROT_READ | PROT_WRITE | PROT_EXEC);
    int err = errno;

    if (rc == 0) {
        log_write("[H004]   mprotect(RWX) SUCCEEDED — reverting immediately, no bytes touched\n");
        int rc2 = mprotect((void*)page_addr, sz, PROT_READ | PROT_EXEC);
        log_write("[H004]   revert to R-X: rc=%d errno=%d (%s)\n",
                  rc2, rc2 == 0 ? 0 : errno, rc2 == 0 ? "ok" : strerror(errno));
    } else {
        log_write("[H004]   mprotect(RWX) FAILED — rc=%d errno=%d (%s)\n",
                  rc, err, strerror(err));
    }
}

// Attempts mach_vm_protect() on the same page (matches FA-001's original API).
static void test_mach_vm_protect(uint64_t page_addr, size_t sz) {
    log_write("[H004] --- mach_vm_protect() test @ 0x%llx (size=%zu) ---\n",
              (unsigned long long)page_addr, sz);

    kern_return_t kr = mach_vm_protect(
        mach_task_self(),
        (mach_vm_address_t)page_addr,
        (mach_vm_size_t)sz,
        FALSE,
        VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE);

    if (kr == KERN_SUCCESS) {
        log_write("[H004]   mach_vm_protect(RWX) SUCCEEDED — reverting immediately, no bytes touched\n");
        kern_return_t kr2 = mach_vm_protect(
            mach_task_self(),
            (mach_vm_address_t)page_addr,
            (mach_vm_size_t)sz,
            FALSE,
            VM_PROT_READ | VM_PROT_EXECUTE);
        log_write("[H004]   revert to R-X: kr=%d (%s)\n", kr2, kr2 == KERN_SUCCESS ? "ok" : "FAILED");
    } else {
        log_write("[H004]   mach_vm_protect(RWX) FAILED — kr=%d (KERN_SUCCESS=0, "
                  "KERN_PROTECTION_FAILURE=2, KERN_INVALID_ADDRESS=1)\n", kr);
    }
}

// ---------------------------------------------------------------------------
// dyld image-load callback
// ---------------------------------------------------------------------------

static void on_image_added(const struct mach_header* mh, intptr_t slide) {
    if (!g_log || mh->magic != MH_MAGIC_64) return;

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

    SegInfo text;
    find_text_segment(mh64, &text);
    if (!text.vmsize) {
        log_write("[H004] ERROR: could not locate __TEXT segment\n");
        return;
    }

    uint64_t text_lo = text.vmaddr + (uint64_t)slide;
    uint64_t text_hi = text_lo + text.vmsize;

    log_write("[H004] ========== Cyberpunk2077 binary detected ==========\n");
    log_write("[H004]   path    = %s\n", path);
    log_write("[H004]   base    = 0x%016llx\n", (unsigned long long)(uintptr_t)mh);
    log_write("[H004]   slide   = 0x%016llx\n", (unsigned long long)(uint64_t)slide);
    log_write("[H004]   __TEXT  = [0x%llx, 0x%llx)  size=%llu KB\n",
              (unsigned long long)text_lo, (unsigned long long)text_hi,
              (unsigned long long)((text_hi - text_lo) / 1024));

    long ps = page_size();
    // Pick a page well inside __TEXT (1 MiB in), away from the Mach-O header
    // and load commands at the very start of the segment. Page-align it.
    uint64_t candidate = text_lo + 0x100000ULL;
    uint64_t page_addr = candidate & ~((uint64_t)ps - 1);
    if (page_addr + (uint64_t)ps > text_hi) {
        // Segment smaller than expected — fall back to first full page after header.
        page_addr = (text_lo + (uint64_t)ps) & ~((uint64_t)ps - 1);
    }
    log_write("[H004]   test page = 0x%llx (page_size=%ld)\n",
              (unsigned long long)page_addr, ps);
    log_write("[H004]   NOTE: no bytes will be written to this page under any outcome.\n");

    test_mach_vm_protect(page_addr, (size_t)ps);
    test_mprotect(page_addr, (size_t)ps);

    log_write("[H004] ========== probe complete — H-004/T-005 result recorded above ==========\n");
    fflush(g_log);
}

// ---------------------------------------------------------------------------
// Constructor — entry point on dylib load
// ---------------------------------------------------------------------------

__attribute__((constructor))
static void h004_probe_init(void) {
    g_log = fopen(k_log, "w");
    if (!g_log) return;
    log_ts("dylib loaded");
    log_write("[H004] log = %s\n", k_log);
    log_write("[H004] pid = %d\n", (int)getpid());
    _dyld_register_func_for_add_image(on_image_added);
    log_write("[H004] dyld image callback registered\n");
    fflush(g_log);
}
