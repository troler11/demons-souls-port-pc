/* The old wrapper menus are removed from the executable. Keep the small ABI
 * used by the title hooks; a replacement frontend can launch with arguments
 * and environment variables without loading RmlUi/FreeType/D3D11On12 UI. */
#include "ppu_recomp.h"
#include "ui/include/des_ui.h"
#include <cstdio>

extern "C" PPU_THREAD_LOCAL void (*g_trampoline_fn)(void*);

extern "C" {
void (*g_des_ui_sound)(int) = nullptr;

void des_ui_early_init(void)
{
    std::fprintf(stderr, "[ui] Wrapper menus removed; direct game startup\n");
}

int des_ui_start_screen_up(void) { return 0; }
void des_menu_sfx_init(void) {}

/* Keep the original guest menu update. Only the wrapper's sound-request
 * queue is removed; the game's own menus and audio still execute normally. */
void des_menu_tick(ppu_context* ctx)
{
    ctx->gpr[9] = vm_read32(vm_read32((uint32_t)ctx->gpr[2] - 0x72F0u) - 0x8000u);
    ctx->gpr[3] = vm_read32((uint32_t)ctx->gpr[9]);
    g_trampoline_fn = (void (*)(void*))func_00724AD0;
}
}
