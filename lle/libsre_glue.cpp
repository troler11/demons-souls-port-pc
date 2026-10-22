/*
 * Demon's Souls port -- loads the real SPURS kernel (libsre, lifted from the
 * user's own decrypted firmware) instead of the runtime's HLE cellSpurs.
 *
 * The HLE scheduler finished some of the title's Havok SPU tasks without their
 * exit path, so the SPU-to-PPU completion events the Havok PPU thread counts
 * never all arrived and the loader stopped at a random point. The lifted kernel
 * runs the real task life cycle (docs/FIRMWARE_LLE.md).
 *
 * Built in its own translation unit because the lifted code has its own
 * ppu_recomp.h (libsre_recomp.h); the game's port code only sees the C entry
 * points below. Nothing derived from firmware is checked in anywhere.
 */
#include "libsre_recomp.h"
#include "prx/prx_loader.h"

#include <cstdio>
#include <cstdlib>

extern "C" void ppu_register_function(uint64_t addr, void (*fn)(ppu_context*));

static const prx_export s_libsre_exports[] = {
#include "libsre_exports.inc"
};

static void reg(uint32_t guest_addr, void (*host)(void*))
{
    ppu_register_function(guest_addr, (void (*)(ppu_context*))host);
}

/* Loads the relocated image at its lift base and publishes its exports, after
 * which ps3_hle_call dispatches every exported NID (cellSpurs, cellSync, ...)
 * into the real code. Returns 1 on success. */
extern "C" int des_libsre_load(const char* image_path)
{
    uint32_t size = 0;
    uint8_t* image = prx_image_load_file(image_path, &size);
    if (!image) return 0;
    prx_module m = {};
    m.name         = "libsre";
    m.base         = 0x30000000u;
    m.image        = image;
    m.image_size   = size;
    m.funcs        = (const prx_func_entry*)libsre_function_table;   /* same layout */
    m.func_count   = libsre_function_table_count;
    m.exports      = s_libsre_exports;
    m.export_count = (uint32_t)(sizeof s_libsre_exports / sizeof s_libsre_exports[0]);
    prx_load_result r = prx_load_module(&m, reg);
    return r.ok;
}
