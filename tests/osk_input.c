/* Real OSK implementation, native Windows EDIT/dialog, small BE guest VM. */
#include "../third_party/ps3recomp/libs/misc/cellOskDialog.c"
#include <assert.h>
static uint8_t memory[8192];
uint8_t* vm_base = memory;
int g_resv_store_active;
uint32_t g_ww_lo, g_ww_hi;
int spu_coh_is_reserved(uint32_t addr) { (void)addr; return 0; }
void spu_lockline_lock(void) {}
void spu_lockline_unlock(void) {}
void spu_coh_notify_write(uint32_t addr) { (void)addr; }
void ppu_resv_break_store(uint64_t addr) { (void)addr; }
void ps3_ww_report_inline(uint32_t a, uint64_t v, int w) { (void)a; (void)v; (void)w; }
static LONG hold;
void cellPad_set_host_hold(int value) { InterlockedExchange(&hold, value); }
static u32 events[256], count;
void cellSysutilQueueEventAll(uint32_t status, uint32_t param) { (void)param; assert(count < 256); events[count++] = status; }
#define INFO ((CellOskDialogInputFieldInfo*)(uintptr_t)16)
#define OUT ((CellOskDialogCallbackReturnParam*)(uintptr_t)64)
static void setup(u32 limit, const WCHAR* initial)
{
    vm_write32(16, 256); vm_write32(20, 1024); vm_write32(24, limit);
    const WCHAR* prompt = L"Digite o nome do personagem:";
    for (unsigned i = 0; ; i++) { vm_write16(256 + i * 2, prompt[i]); if (!prompt[i]) break; }
    for (unsigned i = 0; ; i++) { vm_write16(1024 + i * 2, initial[i]); if (!initial[i]) break; }
    vm_write32(68, 32); vm_write32(72, 2048); memset(memory + 2048, 0xa5, 200);
}
static HWND opened(void)
{
    for (unsigned tries = 0; tries < 500; tries++) {
        OSK_LOCK(); HWND window = s_window; OSK_UNLOCK();
        if (window && GetDlgItem(window, 100) && GetDlgItem(window, IDCANCEL)) {
            SendMessageW(window, WM_NULL, 0, 0); return window;
        }
        Sleep(10);
    }
    assert(!"native dialog not opened"); return NULL;
}
static void idle(void)
{
    for (unsigned tries = 0; tries < 500; tries++) {
        OSK_LOCK(); unsigned workers = s_workers; OSK_UNLOCK();
        if (!workers) { assert(!hold); cellOskDialog_tick(); return; } Sleep(10);
    }
    assert(!"worker leaked or deadlocked");
}
static void text_equals(const WCHAR* expected)
{
    for (unsigned i = 0; ; i++) { assert(vm_read16(2048 + i * 2) == expected[i]); if (!expected[i]) break; }
}
int main(void)
{
    _putenv_s("PS3_NO_USER_INPUT", "1"); _putenv_s("PS3_OSK_TEXT", "Jo\xc3\xa3o");
    setup(16, L""); assert(cellOskDialogLoadAsync(0, NULL, INFO) == CELL_OK);
    cellOskDialog_tick();
    assert(count == 2 && events[0] == OSK_LOADED && events[1] == OSK_FINISHED);
    assert(cellOskDialogLoadAsync(0, NULL, INFO) == CELL_OSK_ERROR_DIALOG_ALREADY_LOADED);
    assert(cellOskDialogGetInputText(OUT) == CELL_OK); text_equals(L"Jo\u00e3o"); assert(vm_read32(68) == 32);
    vm_write32(68, 2); assert(cellOskDialogUnloadAsync(OUT) == CELL_OK); text_equals(L"Jo"); assert(vm_read16(2054) == 0x006f); // prior full result, no write beyond terminator
    assert(events[2] == OSK_UNLOADED);
    _putenv_s("PS3_OSK_TEXT", "abcdef"); setup(3, L""); assert(cellOskDialogLoadAsync(0, NULL, INFO) == CELL_OK);
    assert(cellOskDialogUnloadAsync(OUT) == CELL_OK); text_equals(L"abc"); assert(vm_read16(2056) == 0xa5a5);
    u16 dst[5], pair[] = {0xd83d,0xde00,'x',0}; assert(osk_copy(pair,dst,1) == 0 && !dst[0]); assert(osk_copy(pair,dst,2) == 2 && !dst[2]);
    osk_utf8("\xf0\x9f\x98\x80x",dst,2); assert(dst[0] == 0xd83d && dst[1] == 0xde00 && !dst[2]);
    _putenv_s("PS3_OSK_TEXT", ""); _putenv_s("PS3_NO_USER_INPUT", "");
    setup(16, L""); assert(cellOskDialogLoadAsync(0, NULL, INFO) == CELL_OK); HWND window = opened();
    assert(hold && !IsWindowEnabled(GetDlgItem(window, IDOK)));
    assert(cellOskDialogGetInputText(OUT) == CELL_OK && vm_read32(64) == CELL_OSK_DIALOG_INPUT_FIELD_RESULT_NO_INPUT_TEXT);
    OSK_LOCK(); assert(!s_finished && events[count-1] == OSK_LOADED); OSK_UNLOCK();
    SetDlgItemTextW(window, 100, L"Jo\u00e3o Silva"); assert(IsWindowEnabled(GetDlgItem(window, IDOK)));
    PostMessageW(GetDlgItem(window,100), WM_KEYDOWN, VK_RETURN, 0); idle();
    assert(cellOskDialogUnloadAsync(OUT) == CELL_OK && vm_read32(64) == 0); text_equals(L"Jo\u00e3o Silva");
    setup(16, L"Nome anterior"); assert(cellOskDialogLoadAsync(0, NULL, INFO) == CELL_OK); window = opened();
    WCHAR before[32]; GetDlgItemTextW(window,100,before,32); assert(!wcscmp(before,L"Nome anterior"));
    SetDlgItemTextW(window,100,L"Outro nome"); PostMessageW(GetDlgItem(window,100),WM_KEYDOWN,VK_ESCAPE,0); idle();
    assert(cellOskDialogUnloadAsync(OUT) == CELL_OK && vm_read32(64) == 1 && vm_read16(2048) == 0xa5a5);
    setup(3,L""); assert(cellOskDialogLoadAsync(0,NULL,INFO) == CELL_OK); window=opened();
    SetDlgItemTextW(window,100,L"abcdef"); PostMessageW(window,WM_COMMAND,IDOK,0); idle();
    assert(cellOskDialogUnloadAsync(OUT) == CELL_OK); text_equals(L"abc"); assert(vm_read16(2056) == 0xa5a5);
    setup(16,L""); assert(cellOskDialogLoadAsync(0,NULL,INFO) == CELL_OK); opened();
    assert(cellOskDialogAbort() == CELL_OK); u32 oldgen=s_generation;
    assert(cellOskDialogUnloadAsync(OUT) == CELL_OK && vm_read32(64) == 2);
    setup(16,L""); assert(cellOskDialogLoadAsync(0,NULL,INFO) == CELL_OK); window=opened();
    OSK_LOCK(); osk_finish(oldgen,(const u16*)L"stale",0); assert(!s_finished); OSK_UNLOCK();
    PostMessageW(window,WM_CLOSE,0,0); idle(); assert(cellOskDialogUnloadAsync(OUT) == CELL_OK && vm_read32(64) == 1);
    DWORD handles_before,handles_after; GetProcessHandleCount(GetCurrentProcess(),&handles_before);
    for(unsigned i=0;i<12;i++) { setup(16,L""); assert(cellOskDialogLoadAsync(0,NULL,INFO) == CELL_OK); opened(); assert(cellOskDialogEnd() == CELL_OK); idle(); }
    GetProcessHandleCount(GetCurrentProcess(),&handles_after); assert(handles_after<=handles_before+3);
    setup(0,L""); assert(cellOskDialogLoadAsync(0,NULL,INFO) == CELL_OSK_ERROR_INVALID_PARAMETER);
    puts("PASS: native EDIT/Enter/Escape, pending events, Unicode, limits/BE/canaries, cancel, abort/stale generation and 12 shutdown/handle cycles");
    return 0;
}
