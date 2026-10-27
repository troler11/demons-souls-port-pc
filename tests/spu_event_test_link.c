/* Unused game/PPU linkage for an isolated SPU channel executable.
 * Every function aborts: this test must stay within the channel runtime. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
void* vm_base;void* g_active_ctx;uint64_t ppu_vm_size;
uint32_t g_barrier_sync_watch,g_resv_store_active,g_ww_hi,g_ww_lo;
uint32_t g_spu_image_src_ea[256],g_spu_image_span[256],g_spu_image_ls_start[256];
const uint32_t function_table_count=0;const uintptr_t function_table[2]={0};
#define UNUSED_HOOK(name) void name(void){fprintf(stderr,"Unexpected game hook: %s\n",#name);abort();}
UNUSED_HOOK(lbp_breadcrumb_dump)
UNUSED_HOOK(ppu_dump_bctrl_ring)
UNUSED_HOOK(ppu_dump_guest_stack)
UNUSED_HOOK(ppu_guard_page)
UNUSED_HOOK(ppu_guest_caller)
UNUSED_HOOK(ppu_guest_callstack)
UNUSED_HOOK(ppu_log_host_chain)
UNUSED_HOOK(ppu_resv_break_store)
UNUSED_HOOK(ppu_resv_register)
UNUSED_HOOK(ppu_vfs_root)
UNUSED_HOOK(ps3_hle_register_ctx)
UNUSED_HOOK(ps3_name_host_thread)
UNUSED_HOOK(ps3_sampler_note_thread)
UNUSED_HOOK(ps3_spu_image_source_ea)
UNUSED_HOOK(ps3_ww_report_inline)
UNUSED_HOOK(vm_read32)
UNUSED_HOOK(vm_write32)
UNUSED_HOOK(vm_write8)
