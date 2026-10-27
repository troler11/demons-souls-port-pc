// Exercise the real input implementation against a small guest-memory buffer.
#include "../third_party/ps3recomp/libs/input/cellPad.c"

static uint8_t memory[2048];
uint8_t* vm_base = memory;
int g_resv_store_active;
uint32_t g_ww_lo, g_ww_hi;
int spu_coh_is_reserved(uint32_t addr) { (void)addr; return 0; }
void spu_lockline_lock(void) {}
void spu_lockline_unlock(void) {}
void spu_coh_notify_write(uint32_t addr) { (void)addr; }
void ppu_resv_break_store(uint64_t addr) { (void)addr; }
void ps3_ww_report_inline(uint32_t addr, uint64_t val, int width)
{ (void)addr; (void)val; (void)width; }
int sail_movie_active(void) { return 0; }

static unsigned press_cross(void) { return 0x4000; }
static unsigned word(unsigned offset)
{
    const unsigned addr = 64 + 4 + offset * 2;
    return (unsigned)memory[addr] * 256u + memory[addr + 1];
}

int main(void)
{
    char temp[MAX_PATH], input[MAX_PATH];
    if (!GetTempPathA(sizeof temp, temp) || !GetTempFileNameA(temp, "pad", 0, input)) return 10;
    _putenv_s("PAD_FILE", input);
    _putenv_s("PAD_NO_HOST", "1");
    _putenv_s("PAD_NO_HID", "1");
    _putenv_s("PAD_NO_KEYBOARD", "1");
    if (cellPadInit(4) != CELL_OK) return 1;
    for (int i = 0; i < 10; i++) {
        Sleep(5);
        if (cellPadGetData(0, (CellPadData*)(uintptr_t)64) != CELL_OK) return 2;
        if (word(CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_X) != 128 ||
            word(CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_Y) != 128 ||
            word(CELL_PAD_BTN_OFFSET_ANALOG_LEFT_X) != 128 ||
            word(CELL_PAD_BTN_OFFSET_ANALOG_LEFT_Y) != 128 ||
            word(CELL_PAD_BTN_OFFSET_DIGITAL1) != 0 ||
            word(CELL_PAD_BTN_OFFSET_DIGITAL2) != 0) return 3;
    }
    g_pad_title_hook = press_cross;
    Sleep(5);
    if (cellPadGetData(0, (CellPadData*)(uintptr_t)64) != CELL_OK ||
        word(CELL_PAD_BTN_OFFSET_DIGITAL2) != 0x40) return 4;
    g_pad_title_hook = NULL;
    FILE* f = fopen(input,"wb"); if (!f) return 11;
    fputs("0 3 0 255 64 192\n", f); fclose(f);
    /* A second controller must not consume the command. */
    cellPadGetData(1, (CellPadData*)(uintptr_t)64);
    for (int i=0; i<3; i++) {
        cellPadGetData(0, (CellPadData*)(uintptr_t)64);
        if (word(CELL_PAD_BTN_OFFSET_ANALOG_LEFT_X)!=0 || word(CELL_PAD_BTN_OFFSET_ANALOG_LEFT_Y)!=255 ||
            word(CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_X)!=64 || word(CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_Y)!=192) return 12;
    }
    cellPadGetData(0, (CellPadData*)(uintptr_t)64);
    if (word(CELL_PAD_BTN_OFFSET_ANALOG_LEFT_X)!=128 || word(CELL_PAD_BTN_OFFSET_ANALOG_LEFT_Y)!=128 ||
        word(CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_X)!=128 || word(CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_Y)!=128) return 13;
    f=fopen(input,"rb"); if (!f || fgetc(f)!=EOF) return 14; fclose(f);
    f=fopen(input,"wb"); if (!f) return 15; fputs("0x4000 2\n",f); fclose(f);
    for(int i=0;i<2;i++) {cellPadGetData(0,(CellPadData*)(uintptr_t)64);if(word(CELL_PAD_BTN_OFFSET_DIGITAL2)!=0x40)return 16;}
    cellPadGetData(0,(CellPadData*)(uintptr_t)64); if(word(CELL_PAD_BTN_OFFSET_DIGITAL2))return 17;
    f=fopen(input,"wb"); if(!f)return 18;fputs("0x4000 2 256 128 128 128\n",f);fclose(f);
    cellPadGetData(0,(CellPadData*)(uintptr_t)64);
    if(word(CELL_PAD_BTN_OFFSET_DIGITAL2) || word(CELL_PAD_BTN_OFFSET_ANALOG_LEFT_X)!=128)return 19;
    f=fopen(input,"wb");if(!f)return 20;fclose(f);
    cellPad_set_host_hold(1);
    Sleep(5);
    if (cellPadGetData(0, (CellPadData*)(uintptr_t)64) != CELL_OK ||
        word(CELL_PAD_BTN_OFFSET_DIGITAL2) != 0 ||
        word(CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_X) != 128) return 5;
    cellPadEnd();
    DeleteFileA(input);
    puts("PASS: neutral pad, analog file expiry, port isolation, legacy buttons, invalid axes and host hold");
    return 0;
}
