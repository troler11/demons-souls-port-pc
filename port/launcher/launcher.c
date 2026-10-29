/* Launcher: the game's settings in one window, then Play through
 * scripts\windows\play.ps1. Win32 + GDI+, no other dependency.
 *
 * Layout after the inFAMOUS 2 recomp launcher (erikomis/infamous2-recomp,
 * MIT): a sidebar with six pages, settings as rows (text on the left, a list,
 * switch or slider on the right) saved the moment they change, a home page
 * with the player's own art, quick settings, the last save and the status.
 *
 *   Início       hero (PIC1.PNG from the user's disc), JOGAR / PARAR, cards
 *   Gráficos     window, internal resolution, FSR, FXAA, sharpening
 *   Desempenho   frame rate, vsync, frame generation
 *   Jogo         movies, intro, continue, RPCS3 folder, the saves manager
 *   Controles    keyboard / controller bindings (work\input.ini, PAD_KEYMAP)
 *   Avançado     the game log, extra environment variables, the command
 *
 * Nothing from the game ships with it: art and save details are read from the
 * user's files. Settings: work\launcher.ini; bindings: work\input.ini; extra
 * variables: work\launcher_env.txt. DES_LAUNCHER_SHOT=<png> (with
 * DES_LAUNCHER_PAGE=<0-5>) renders one page to a PNG and exits. */
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <commctrl.h>
#include <uxtheme.h>
#include <xinput.h>
#include <ctype.h>
#include <stdio.h>
#include <wchar.h>

/* ---- GDI+ flat API (the SDK headers are C++ only) ---------------------- */
typedef int GpStatus;
typedef float REAL;
typedef DWORD ARGB;
typedef struct GpGraphics GpGraphics;
typedef struct GpImage GpImage;
typedef struct GpBrush GpBrush;
typedef struct GpPen GpPen;
typedef struct GpPath GpPath;
typedef struct GpFont GpFont;
typedef struct GpFontFamily GpFontFamily;
typedef struct GpFontCollection GpFontCollection;
typedef struct GpStringFormat GpStringFormat;
typedef struct { REAL X, Y; } GpPointF;
typedef struct { REAL X, Y, Width, Height; } GpRectF;
typedef struct { UINT32 GdiplusVersion; void* DebugEventCallback; BOOL SuppressBackgroundThread; BOOL SuppressExternalCodecs; } GdiplusStartupInput;
#define GDIP __stdcall
GpStatus GDIP GdiplusStartup(ULONG_PTR*, const GdiplusStartupInput*, void*);
void GDIP GdiplusShutdown(ULONG_PTR);
GpStatus GDIP GdipLoadImageFromFile(const WCHAR*, GpImage**);
GpStatus GDIP GdipGetImageWidth(GpImage*, UINT*);
GpStatus GDIP GdipGetImageHeight(GpImage*, UINT*);
GpStatus GDIP GdipDisposeImage(GpImage*);
GpStatus GDIP GdipCreateBitmapFromScan0(INT, INT, INT, INT, BYTE*, GpImage**);
GpStatus GDIP GdipGetImageGraphicsContext(GpImage*, GpGraphics**);
GpStatus GDIP GdipCreateFromHDC(HDC, GpGraphics**);
GpStatus GDIP GdipDeleteGraphics(GpGraphics*);
GpStatus GDIP GdipSetSmoothingMode(GpGraphics*, INT);
GpStatus GDIP GdipSetTextRenderingHint(GpGraphics*, INT);
GpStatus GDIP GdipSetInterpolationMode(GpGraphics*, INT);
GpStatus GDIP GdipSetPixelOffsetMode(GpGraphics*, INT);
GpStatus GDIP GdipDrawImageI(GpGraphics*, GpImage*, INT, INT);
GpStatus GDIP GdipDrawImageRectI(GpGraphics*, GpImage*, INT, INT, INT, INT);
GpStatus GDIP GdipDrawImageRectRectI(GpGraphics*, GpImage*, INT, INT, INT, INT, INT, INT, INT, INT, INT, const void*, void*, void*);
GpStatus GDIP GdipCreateSolidFill(ARGB, GpBrush**);
GpStatus GDIP GdipCreateLineBrush(const GpPointF*, const GpPointF*, ARGB, ARGB, INT, GpBrush**);
GpStatus GDIP GdipDeleteBrush(GpBrush*);
GpStatus GDIP GdipFillRectangle(GpGraphics*, GpBrush*, REAL, REAL, REAL, REAL);
GpStatus GDIP GdipFillEllipse(GpGraphics*, GpBrush*, REAL, REAL, REAL, REAL);
GpStatus GDIP GdipCreatePen1(ARGB, REAL, INT, GpPen**);
GpStatus GDIP GdipDeletePen(GpPen*);
GpStatus GDIP GdipDrawLine(GpGraphics*, GpPen*, REAL, REAL, REAL, REAL);
GpStatus GDIP GdipCreatePath(INT, GpPath**);
GpStatus GDIP GdipDeletePath(GpPath*);
GpStatus GDIP GdipAddPathArc(GpPath*, REAL, REAL, REAL, REAL, REAL, REAL);
GpStatus GDIP GdipClosePathFigure(GpPath*);
GpStatus GDIP GdipFillPath(GpGraphics*, GpBrush*, GpPath*);
GpStatus GDIP GdipDrawPath(GpGraphics*, GpPen*, GpPath*);
GpStatus GDIP GdipSetClipRect(GpGraphics*, REAL, REAL, REAL, REAL, INT);
GpStatus GDIP GdipResetClip(GpGraphics*);
GpStatus GDIP GdipNewPrivateFontCollection(GpFontCollection**);
GpStatus GDIP GdipPrivateAddFontFile(GpFontCollection*, const WCHAR*);
GpStatus GDIP GdipCreateFontFamilyFromName(const WCHAR*, GpFontCollection*, GpFontFamily**);
GpStatus GDIP GdipCreateFont(const GpFontFamily*, REAL, INT, INT, GpFont**);
GpStatus GDIP GdipCreateStringFormat(INT, LANGID, GpStringFormat**);
GpStatus GDIP GdipSetStringFormatAlign(GpStringFormat*, INT);
GpStatus GDIP GdipSetStringFormatLineAlign(GpStringFormat*, INT);
GpStatus GDIP GdipSetStringFormatTrimming(GpStringFormat*, INT);
GpStatus GDIP GdipSaveImageToFile(GpImage*, const WCHAR*, const CLSID*, const void*);
GpStatus GDIP GdipCreateBitmapFromHBITMAP(HBITMAP, HPALETTE, GpImage**);
GpStatus GDIP GdipDrawString(GpGraphics*, const WCHAR*, INT, const GpFont*, const GpRectF*, const GpStringFormat*, const GpBrush*);
GpStatus GDIP GdipMeasureString(GpGraphics*, const WCHAR*, INT, const GpFont*, const GpRectF*, const GpStringFormat*, GpRectF*, INT*, INT*);
enum { UnitPixel = 2, FontRegular = 0, FontBold = 1, SmoothAntiAlias = 4, TextAntiAliasGridFit = 3,
       InterpHighQualityBicubic = 7, PixelOffsetHighQuality = 2, WrapTileFlipXY = 3,
       AlignNear = 0, AlignCenter = 1, AlignFar = 2, Format32bppPARGB = 0x000E200B,
       StringNoWrap = 0x00001000, TrimEllipsis = 3, CombineReplace = 0 };
/* An image decoded into memory: GdipLoadImageFromFile keeps the file open for
 * the image's lifetime, and the launcher held the save's ICON0.PNG locked --
 * the game (and the test scripts' save restore) could not replace it. */
static GpImage* load_image_copy(const WCHAR* path)
{
    GpImage* file = NULL; GpImage* copy = NULL; GpGraphics* gr = NULL; UINT w = 0, h = 0;
    if (GdipLoadImageFromFile(path, &file) != 0 || !file) return NULL;
    GdipGetImageWidth(file, &w); GdipGetImageHeight(file, &h);
    if (w && h && GdipCreateBitmapFromScan0((INT)w, (INT)h, 0, 0x26200A /* 32bppARGB */, NULL, &copy) == 0 &&
        GdipGetImageGraphicsContext(copy, &gr) == 0) {
        GdipDrawImageRectI(gr, file, 0, 0, (INT)w, (INT)h);
        GdipDeleteGraphics(gr);
    } else if (copy) { GdipDisposeImage(copy); copy = NULL; }
    GdipDisposeImage(file);
    return copy;
}

/* ---- layout (logical units; g.s scales to pixels) ----------------------- */
#define WIN_W 1280
#define WIN_H 800
#define SIDE_W 224
#define PAGE_X (SIDE_W + 36)
#define PAGE_W (WIN_W - SIDE_W - 72)
#define CTRL_W 240

static const ARGB kBg = 0xFF0B0A09, kSide = 0xFF100E0C, kCard = 0xFF14120F, kLine = 0xFF24201B,
                  kBorder = 0xFF2E2923, kGold = 0xFFC9AA71, kText = 0xFFE8E2D4, kDim = 0xFF9A9286,
                  kFaint = 0xFF6E675E, kOrange = 0xFFE0874A, kGreen = 0xFF6FBF73, kRed = 0xFFD9614F;

/* ---- settings ------------------------------------------------------------ */
enum { T_LIST, T_TOGGLE, T_SLIDER };
typedef struct {
    const wchar_t* key;        /* in launcher.ini */
    const wchar_t* label;
    const wchar_t* help;
    int type, count, def, value;
    const wchar_t* values[6];
} setting;

enum { S_MODE, S_RES, S_SCALE, S_VSYNC, S_FSR, S_FXAA, S_SHARPEN, S_FPS, S_FRAMEGEN, S_SMOOTH, S_DTMAX,
       S_MOVIES, S_SKIPINTRO, S_CONTINUE, S_AUTOBACKUP, S_KEYBOARD, S_LOGFILTER, S_COUNT };
static setting s_set[S_COUNT] = {
    {L"Mode", L"Modo de exibição", L"Janela, ou tela cheia sem borda. No jogo, F11 ou Alt+Enter alterna.",
     T_LIST, 2, 0, 0, {L"Janela", L"Tela cheia"}},
    {L"Resolution", L"Tamanho da janela", L"Tamanho da janela do jogo. Na tela cheia, a imagem ocupa a tela toda.",
     T_LIST, 4, 1, 1, {L"1280 × 720", L"1920 × 1080", L"2560 × 1440", L"3840 × 2160"}},
    {L"Scale", L"Resolução interna", L"Resolução em que a cena é desenhada. Automática acompanha a janela; com FSR, desenha cerca de 1,5x menor e amplia.",
     T_LIST, 5, 0, 0, {L"Automática", L"1x · 720p", L"2x · 1440p", L"3x · 2160p", L"4x · 2880p"}},
    {L"Vsync", L"Sincronização vertical", L"Espera o monitor a cada quadro: sem rasgos na imagem, com um pouco mais de atraso. O limite de quadros continua valendo.",
     T_TOGGLE, 2, 0, 0, {0}},
    {L"Fsr", L"AMD FSR 1", L"Desenha menor e amplia com AMD FSR 1. Ajuda quando o limite é a GPU; não acelera o trabalho de CPU do jogo.",
     T_TOGGLE, 2, 0, 0, {0}},
    {L"Fxaa", L"Antisserrilhamento FXAA", L"Suaviza as bordas serrilhadas com custo baixo de GPU; deixa a imagem um pouco mais suave.",
     T_TOGGLE, 2, 0, 0, {0}},
    {L"SharpenPct", L"Nitidez", L"Nitidez adaptativa (AMD RCAS). Desligada em 0; com FSR ligado e nitidez em 0, usa 80%.",
     T_SLIDER, 101, 0, 0, {0}},
    {L"Fps", L"Taxa de quadros", L"60 é o padrão do port. 30 é o modo original do PS3. Sem limite é experimental: o jogo foi feito para 30/60.",
     T_LIST, 3, 1, 1, {L"30 · original", L"60", L"Sem limite"}},
    {L"FrameGen", L"Frame generation", L"Experimental: a GPU cria um quadro entre cada dois estimando o movimento pela imagem (60 → 120 na tela). Pode deixar rastros e acrescenta atraso.",
     T_TOGGLE, 2, 0, 0, {0}},
    {L"DtSmooth", L"Suavizar movimento", L"O jogo avança pela média dos últimos quadros em vez do último, então a variação de tempo entre quadros deixa de virar tranco no movimento. O tempo total não muda. Vale no modo 60.",
     T_LIST, 4, 0, 0, {L"Desligado", L"Leve (2)", L"Médio (4)", L"Forte (8)"}},
    {L"DtMax", L"Limite de salto por quadro", L"Quanto o jogo pode avançar num quadro que demorou. Menor: um inimigo rápido salta menos de lugar quando o jogo engasga, ao custo de uma desaceleração breve. Vale no modo 60.",
     T_LIST, 3, 0, 0, {L"50 ms · padrão", L"33 ms", L"25 ms"}},
    {L"Movies", L"Cinemáticas em vídeo", L"Toca as cinemáticas pré-renderizadas (precisa do ffmpeg). Desligadas, o jogo as pula.",
     T_TOGGLE, 2, 1, 1, {0}},
    {L"SkipIntro", L"Encurtar logos de abertura", L"Pula a maior parte dos logos antes do menu principal.",
     T_TOGGLE, 2, 0, 0, {0}},
    {L"AutoContinue", L"Continuar direto do último save", L"Passa pelos menus sozinho e carrega o último personagem salvo.",
     T_TOGGLE, 2, 0, 0, {0}},
    {L"AutoBackup", L"Backup automático ao jogar", L"Copia os saves para work\\save_backups antes de cada partida. Guarda os 10 mais recentes.",
     T_TOGGLE, 2, 1, 1, {0}},
    {L"Keyboard", L"Teclado e mouse como controle", L"Usa o teclado (e os botões do mouse associados) quando nenhum controle Xbox está conectado.",
     T_TOGGLE, 2, 1, 1, {0}},
    {L"LogFilter", L"Só avisos e erros", L"", T_TOGGLE, 2, 0, 0, {0}},
};
static const wchar_t* const kResArg[] = {L"720p", L"1080p", L"1440p", L"4k"};

/* ---- bindings (the runtime's PAD_KEYMAP format, libs/input/cellPad.c) --- */
enum { KM_BUTTONS = 16, KM_KEYS = 24, XS_NONE = -1, XS_COUNT = 16 };
static const char* const k_km_key[KM_KEYS] = {
    "cross", "circle", "square", "triangle", "l1", "r1", "l2", "r2",
    "l3", "r3", "start", "select", "up", "down", "left", "right",
    "ls_up", "ls_down", "ls_left", "ls_right", "rs_up", "rs_down", "rs_left", "rs_right" };
static const wchar_t* const k_km_label[KM_KEYS] = {
    L"✕  Cruz", L"○  Círculo", L"□  Quadrado", L"△  Triângulo", L"L1", L"R1", L"L2", L"R2",
    L"L3", L"R3", L"START", L"SELECT", L"Direcional ↑", L"Direcional ↓", L"Direcional ←", L"Direcional →",
    L"Analógico esq. ↑", L"Analógico esq. ↓", L"Analógico esq. ←", L"Analógico esq. →",
    L"Analógico dir. ↑", L"Analógico dir. ↓", L"Analógico dir. ←", L"Analógico dir. →" };
static const char* const k_xs_key[XS_COUNT] = {
    "A", "B", "X", "Y", "LB", "RB", "LT", "RT", "LS", "RS", "START", "BACK", "UP", "DOWN", "LEFT", "RIGHT" };
static const wchar_t* const k_xs_label[XS_COUNT] = {
    L"A", L"B", L"X", L"Y", L"LB", L"RB", L"LT", L"RT", L"LS (clique)", L"RS (clique)", L"Start", L"Back",
    L"D-pad ↑", L"D-pad ↓", L"D-pad ←", L"D-pad →" };
/* Up to two keys per control. The built-in layout is the runtime's own. */
static const int k_km_vk_def[KM_KEYS][2] = {
    {'Z'}, {'X'}, {'A'}, {'S'}, {'Q'}, {'W'}, {'1'}, {'2'}, {0}, {0}, {VK_RETURN}, {VK_TAB},
    {VK_UP}, {VK_DOWN}, {VK_LEFT}, {VK_RIGHT}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0} };
/* "Layout PC": WASD moves, the arrows turn the camera, Enter/E confirm, the
 * mouse attacks. Esc is not usable: it closes the game window, and F1/Esc
 * open the port's menu. */
static const int k_km_vk_pc[KM_KEYS][2] = {
    {VK_RETURN, 'E'}, {VK_SPACE, VK_BACK}, {'R'}, {'F'}, {VK_LSHIFT}, {VK_LBUTTON}, {VK_LCONTROL}, {VK_RBUTTON},
    {0}, {VK_MBUTTON, 'Q'}, {VK_TAB}, {'G'}, {'1'}, {'2'}, {'3'}, {'4'},
    {'W'}, {'S'}, {'A'}, {'D'}, {VK_UP}, {VK_DOWN}, {VK_LEFT}, {VK_RIGHT} };
static int s_km_vk[KM_KEYS][2];
static int km_held(int r) { return (s_km_vk[r][0] && (GetAsyncKeyState(s_km_vk[r][0]) & 0x8000)) ||
                                   (s_km_vk[r][1] && (GetAsyncKeyState(s_km_vk[r][1]) & 0x8000)); }
static int s_km_xs[KM_BUTTONS];
static const WORD k_xs_bit[XS_COUNT] = {
    XINPUT_GAMEPAD_A, XINPUT_GAMEPAD_B, XINPUT_GAMEPAD_X, XINPUT_GAMEPAD_Y,
    XINPUT_GAMEPAD_LEFT_SHOULDER, XINPUT_GAMEPAD_RIGHT_SHOULDER, 0, 0,
    XINPUT_GAMEPAD_LEFT_THUMB, XINPUT_GAMEPAD_RIGHT_THUMB, XINPUT_GAMEPAD_START, XINPUT_GAMEPAD_BACK,
    XINPUT_GAMEPAD_DPAD_UP, XINPUT_GAMEPAD_DPAD_DOWN, XINPUT_GAMEPAD_DPAD_LEFT, XINPUT_GAMEPAD_DPAD_RIGHT };

/* ---- saves -------------------------------------------------------------- */
typedef struct {
    wchar_t dir[64], title[128], sub[128], detail[256];
    FILETIME mtime; ULONGLONG size; GpImage* icon;
} save_slot;
typedef struct { wchar_t name[64]; int slots; FILETIME mtime; } save_backup;
#define MAX_SLOTS 16
#define MAX_BACKUPS 24

/* ---- hit regions: rebuilt by every paint, searched topmost first --------- */
enum { R_NAV = 1, R_LIST, R_TOGGLE, R_SLIDER, R_DROPITEM, R_ACTION, R_BIND };
enum { A_PLAY, A_STOP, A_MIN, A_CLOSE, A_ALLGFX, A_MANAGE_SAVES, A_OPEN_SAVES, A_VIEW_LOG, A_OPEN_LOG,
       A_BACKUP_ALL, A_RPCS3, A_CTRL_DEFAULTS, A_ALL_DEFAULTS, A_OPEN_BACKUPS, A_LOG_COPY, A_CTRL_PC,
       A_SLOT_BACKUP = 100, A_SLOT_DELETE = 200, A_BK_RESTORE = 300, A_BK_DELETE = 400 };
#define RID(kind, idx) (((kind) << 16) | (idx))
#define RKIND(id) ((id) >> 16)
#define RIDX(id) ((id) & 0xFFFF)
typedef struct { REAL x, y, w, h; int id; } region;

enum { P_HOME, P_GFX, P_PERF, P_GAME, P_CTRL, P_ADV, P_COUNT };
static const wchar_t* const k_page_name[P_COUNT] = {
    L"INÍCIO", L"GRÁFICOS", L"DESEMPENHO", L"JOGO", L"CONTROLES", L"AVANÇADO" };
static const wchar_t* const k_page_icon[P_COUNT] = {   /* Segoe MDL2 Assets */
    L"\xE80F", L"\xE7F4", L"\xEC4A", L"\xE8F1", L"\xE7FC", L"\xE713" };

static struct {
    HWND hwnd, ed_log, ed_env, ed_cmd;
    HFONT mono;
    HBRUSH edit_bg;
    wchar_t root[MAX_PATH], work[MAX_PATH], ini[MAX_PATH], keymap[MAX_PATH], envfile[MAX_PATH];
    wchar_t rpcs3[MAX_PATH];
    REAL s;
    int w, h;
    GpImage *hero, *icon0;
    GpFontFamily *f_head_fam, *f_text_fam, *f_icon_fam, *f_sym_fam;
    GpFont *f_logo, *f_hero, *f_page, *f_nav, *f_section, *f_label, *f_help, *f_value, *f_small,
           *f_button, *f_icon, *f_sym, *f_tiny;
    GpStringFormat *fmt_left, *fmt_center, *fmt_right, *fmt_wrap, *fmt_ell;
    GpGraphics* measure;                /* for text heights outside paint */
    region reg[512]; int nreg;
    int page, hover, drop, drag, pressed;
    REAL scroll[P_COUNT], content_h[P_COUNT];
    REAL drag_x0, drag_w;
    int cap_row, cap_col; ULONGLONG cap_until;
    WORD pad_prev; int pad_prev_lt, pad_prev_rt;
    BYTE alpha;
    HMODULE xinput;
    DWORD (WINAPI *xget)(DWORD, XINPUT_STATE*);
    HANDLE game;
    save_slot slots[MAX_SLOTS]; int nslots;
    save_backup backups[MAX_BACKUPS]; int nbackups;
    wchar_t title_id[32], app_ver[16], built[64], commit[16];
    ULONGLONG log_size; FILETIME log_time;
    int saved_flash;
    unsigned live_keys;                 /* bitmask of KM rows currently held, for redraws */
} g;

#define S(v) ((REAL)(v) * g.s)

/* ---- files -------------------------------------------------------------- */
static void path_join(wchar_t* out, const wchar_t* a, const wchar_t* b)
{ _snwprintf(out, MAX_PATH, L"%s\\%s", a, b); out[MAX_PATH - 1] = 0; }
static int file_exists(const wchar_t* p) { return GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES; }
static void savedata_dir(wchar_t* out) { path_join(out, g.work, L"gamedata\\dev_hdd0\\home\\00000001\\savedata"); }
static void backups_dir(wchar_t* out) { path_join(out, g.work, L"save_backups"); }

/* The repository root: the exe's folder or its parent (build\DesLauncher.exe). */
static void find_root(void)
{
    wchar_t exe[MAX_PATH], probe[MAX_PATH];
    GetModuleFileNameW(NULL, exe, MAX_PATH);
    *wcsrchr(exe, L'\\') = 0;
    for (int up = 0; up < 2; up++) {
        path_join(probe, exe, L"scripts\\windows\\play.ps1");
        if (file_exists(probe)) break;
        wchar_t* slash = wcsrchr(exe, L'\\');
        if (!slash) break;
        *slash = 0;
    }
    wcscpy(g.root, exe);
    path_join(g.work, g.root, L"work");
    path_join(g.ini, g.work, L"launcher.ini");
    path_join(g.keymap, g.work, L"input.ini");
    path_join(g.envfile, g.work, L"launcher_env.txt");
}

static void fmt_time(const FILETIME* ft, wchar_t* out, int cap)
{
    FILETIME lt; SYSTEMTIME st;
    FileTimeToLocalFileTime(ft, &lt); FileTimeToSystemTime(&lt, &st);
    _snwprintf(out, cap, L"%02d/%02d/%04d, %02d:%02d", st.wDay, st.wMonth, st.wYear, st.wHour, st.wMinute);
}

/* PARAM.SFO: one UTF-8 string by key. */
static int sfo_get(const wchar_t* path, const char* key, wchar_t* out, int cap)
{
    out[0] = 0;
    HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return 0;
    static BYTE buf[65536]; DWORD n = 0;
    ReadFile(f, buf, sizeof buf, &n, NULL); CloseHandle(f);
    if (n < 20 || memcmp(buf, "\0PSF", 4)) return 0;
    #define U16(o) ((DWORD)buf[o] | (DWORD)buf[(o) + 1] << 8)
    #define U32(o) (U16(o) | U16((o) + 2) << 16)
    const DWORD keys = U32(8), data = U32(12), count = U32(16);
    for (DWORD i = 0; i < count && 20 + i * 16 + 16 <= n; i++) {
        const DWORD e = 20 + i * 16, ko = keys + U16(e), fmt = U16(e + 2), len = U32(e + 4), dof = data + U32(e + 12);
        if (ko >= n || strcmp((const char*)buf + ko, key) != 0) continue;
        if ((fmt & 0xFF) != 0x04 || dof + len > n) return 0;
        DWORD l = len; while (l && !buf[dof + l - 1]) l--;
        const int w = MultiByteToWideChar(CP_UTF8, 0, (const char*)buf + dof, (int)l, out, cap - 1);
        out[w > 0 ? w : 0] = 0;
        return 1;
    }
    #undef U32
    #undef U16
    return 0;
}

/* ---- settings and bindings I/O ------------------------------------------ */
static void load_settings(void)
{
    for (int i = 0; i < S_COUNT; i++) {
        int v = (int)GetPrivateProfileIntW(L"launcher", s_set[i].key, -1, g.ini);
        if (i == S_SHARPEN && v < 0) {             /* the old launcher stored 0-10 */
            const int old = (int)GetPrivateProfileIntW(L"launcher", L"Sharpen", -1, g.ini);
            if (old >= 0 && old <= 10) v = old * 10;
        }
        s_set[i].value = v >= 0 && v < s_set[i].count ? v : s_set[i].def;
    }
    /* RPCS3 folder: play.ps1 keeps it in work\settings.json as {"rpcs3": "..."} */
    wchar_t cfg[MAX_PATH]; path_join(cfg, g.work, L"settings.json");
    g.rpcs3[0] = 0;
    FILE* f = _wfopen(cfg, L"rb");
    if (f) {
        char buf[4096]; const size_t n = fread(buf, 1, sizeof buf - 1, f); buf[n] = 0; fclose(f);
        const char* p = strstr(buf, "\"rpcs3\"");
        if (p && (p = strchr(p + 7, '"')) != NULL) {
            char val[MAX_PATH * 2]; int k = 0;
            for (p++; *p && *p != '"' && k < (int)sizeof val - 1; p++) {
                if (*p == '\\' && p[1]) p++;
                val[k++] = *p;
            }
            val[k] = 0;
            MultiByteToWideChar(CP_UTF8, 0, val, -1, g.rpcs3, MAX_PATH);
        }
    }
}
static void save_settings(void)
{
    CreateDirectoryW(g.work, NULL);
    wchar_t v[16];
    for (int i = 0; i < S_COUNT; i++) {
        _snwprintf(v, 16, L"%d", s_set[i].value);
        WritePrivateProfileStringW(L"launcher", s_set[i].key, v, g.ini);
    }
    g.saved_flash = 1;
}
static void save_rpcs3(void)
{
    wchar_t cfg[MAX_PATH]; path_join(cfg, g.work, L"settings.json");
    char u8[MAX_PATH * 3], esc[MAX_PATH * 6]; int k = 0;
    WideCharToMultiByte(CP_UTF8, 0, g.rpcs3, -1, u8, sizeof u8, NULL, NULL);
    for (const char* p = u8; *p && k < (int)sizeof esc - 2; p++) {
        if (*p == '\\' || *p == '"') esc[k++] = '\\';
        esc[k++] = *p;
    }
    esc[k] = 0;
    FILE* f = _wfopen(cfg, L"wb");
    if (f) { fprintf(f, "{\r\n    \"rpcs3\":  \"%s\"\r\n}\r\n", esc); fclose(f); }
}

static void keymap_defaults(void)
{
    memcpy(s_km_vk, k_km_vk_def, sizeof s_km_vk);
    for (int i = 0; i < KM_BUTTONS; i++) s_km_xs[i] = i;   /* XS order matches the buttons */
}
static void keymap_load(void)
{
    keymap_defaults();
    FILE* f = _wfopen(g.keymap, L"r");
    if (!f) return;
    char line[256]; int sect = 0;
    while (fgets(line, sizeof line, f)) {
        char* s = line;
        if (*s == '[') { sect = !strncmp(s, "[keyboard]", 10) ? 1 : !strncmp(s, "[controller]", 12) ? 2 : 0; continue; }
        char* eq = strchr(s, '=');
        if (!eq || *s == ';') continue;
        *eq = 0;
        char* v = eq + 1; v[strcspn(v, "\r\n ")] = 0;
        for (int i = 0; i < KM_KEYS; i++) {
            if (strcmp(s, k_km_key[i])) continue;
            if (sect == 1) {
                char* rest;
                s_km_vk[i][0] = (int)strtol(v, &rest, 10) & 0xFF;
                s_km_vk[i][1] = *rest == ',' ? (int)strtol(rest + 1, NULL, 10) & 0xFF : 0;
            }
            else if (sect == 2 && i < KM_BUTTONS) {
                s_km_xs[i] = XS_NONE;
                for (int x = 0; x < XS_COUNT; x++) if (!_stricmp(v, k_xs_key[x])) s_km_xs[i] = x;
            }
        }
    }
    fclose(f);
}
static void keymap_save(void)
{
    CreateDirectoryW(g.work, NULL);
    FILE* f = _wfopen(g.keymap, L"w");
    if (!f) return;
    fprintf(f, "; Demon's Souls PC: controles (escrito pelo launcher; lido pelo jogo via PAD_KEYMAP)\n");
    fprintf(f, "; [keyboard]: codigo de tecla virtual do Windows, ate duas por controle (0 = nenhuma). [controller]: botao XInput.\n[keyboard]\n");
    for (int i = 0; i < KM_KEYS; i++) {
        if (s_km_vk[i][1]) fprintf(f, "%s=%d,%d\n", k_km_key[i], s_km_vk[i][0], s_km_vk[i][1]);
        else fprintf(f, "%s=%d\n", k_km_key[i], s_km_vk[i][0]);
    }
    fprintf(f, "[controller]\n");
    for (int i = 0; i < KM_BUTTONS; i++) fprintf(f, "%s=%s\n", k_km_key[i], s_km_xs[i] < 0 ? "NONE" : k_xs_key[s_km_xs[i]]);
    fclose(f);
    g.saved_flash = 1;
}

static void key_name(int vk, wchar_t* out, int cap)
{
    static const struct { int vk; const wchar_t* n; } names[] = {
        {VK_LBUTTON, L"Mouse esquerdo"}, {VK_RBUTTON, L"Mouse direito"}, {VK_MBUTTON, L"Mouse meio"},
        {VK_XBUTTON1, L"Mouse 4"}, {VK_XBUTTON2, L"Mouse 5"}, {VK_UP, L"↑"}, {VK_DOWN, L"↓"},
        {VK_LEFT, L"←"}, {VK_RIGHT, L"→"}, {VK_RETURN, L"Enter"}, {VK_SPACE, L"Espaço"},
        {VK_TAB, L"Tab"}, {VK_ESCAPE, L"Esc"}, {VK_BACK, L"Backspace"}, {VK_SHIFT, L"Shift"},
        {VK_CONTROL, L"Ctrl"}, {VK_MENU, L"Alt"}, {VK_LSHIFT, L"Shift esq."}, {VK_RSHIFT, L"Shift dir."},
        {VK_LCONTROL, L"Ctrl esq."}, {VK_RCONTROL, L"Ctrl dir."}, {VK_CAPITAL, L"Caps Lock"},
        {VK_INSERT, L"Insert"}, {VK_DELETE, L"Delete"}, {VK_HOME, L"Home"}, {VK_END, L"End"},
        {VK_PRIOR, L"Page Up"}, {VK_NEXT, L"Page Down"} };
    if (!vk) { wcscpy(out, L"—"); return; }
    for (unsigned i = 0; i < sizeof names / sizeof names[0]; i++)
        if (names[i].vk == vk) { wcsncpy(out, names[i].n, cap); return; }
    const UINT sc = MapVirtualKeyW((UINT)vk, MAPVK_VK_TO_VSC);
    if (!sc || !GetKeyNameTextW((LONG)(sc << 16), out, cap)) _snwprintf(out, cap, L"Tecla %d", vk);
}

/* ---- game info, saves, log ---------------------------------------------- */
static void read_info(void)
{
    wchar_t p[MAX_PATH];
    path_join(p, g.work, L"disc\\PS3_GAME\\PARAM.SFO");
    if (!sfo_get(p, "TITLE_ID", g.title_id, 32)) wcscpy(g.title_id, L"BLUS30443");
    if (!sfo_get(p, "APP_VER", g.app_ver, 16)) sfo_get(p, "VERSION", g.app_ver, 16);
    g.built[0] = 0;
    path_join(p, g.root, L"build\\MyGameRecomp.exe");
    WIN32_FILE_ATTRIBUTE_DATA fa;
    if (GetFileAttributesExW(p, GetFileExInfoStandard, &fa)) fmt_time(&fa.ftLastWriteTime, g.built, 64);
    /* the port's commit, from .git without running git */
    g.commit[0] = 0;
    path_join(p, g.root, L".git\\HEAD");
    FILE* f = _wfopen(p, L"r");
    if (f) {
        char head[256] = {0}; fgets(head, sizeof head, f); fclose(f);
        head[strcspn(head, "\r\n")] = 0;
        char hash[64] = {0};
        if (!strncmp(head, "ref: ", 5)) {
            wchar_t ref[MAX_PATH], rp[MAX_PATH];
            MultiByteToWideChar(CP_UTF8, 0, head + 5, -1, ref, MAX_PATH);
            for (wchar_t* c = ref; *c; c++) if (*c == L'/') *c = L'\\';
            _snwprintf(rp, MAX_PATH, L"%s\\.git\\%s", g.root, ref);
            if ((f = _wfopen(rp, L"r")) != NULL) { fgets(hash, sizeof hash, f); fclose(f); }
        } else strncpy(hash, head, sizeof hash - 1);
        if (strlen(hash) >= 7) { hash[7] = 0; MultiByteToWideChar(CP_UTF8, 0, hash, -1, g.commit, 16); }
    }
}

static ULONGLONG dir_size(const wchar_t* dir)
{
    wchar_t pat[MAX_PATH]; path_join(pat, dir, L"*");
    WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW(pat, &fd); ULONGLONG n = 0;
    if (h == INVALID_HANDLE_VALUE) return 0;
    do if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) n += ((ULONGLONG)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
    while (FindNextFileW(h, &fd));
    FindClose(h);
    return n;
}
static int is_save_dir(const wchar_t* name) { return !wcsncmp(name, L"BLUS30443", 9) || !wcsncmp(name, L"BLES00932", 9) || !wcsncmp(name, L"BCJS", 4); }

static void scan_saves(void)
{
    for (int i = 0; i < g.nslots; i++) if (g.slots[i].icon) GdipDisposeImage(g.slots[i].icon);
    g.nslots = 0; g.nbackups = 0;
    wchar_t dir[MAX_PATH], pat[MAX_PATH], p[MAX_PATH];
    savedata_dir(dir); path_join(pat, dir, L"*");
    WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW(pat, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || fd.cFileName[0] == L'.' || g.nslots == MAX_SLOTS) continue;
            save_slot* s = &g.slots[g.nslots];
            memset(s, 0, sizeof *s);
            wcsncpy(s->dir, fd.cFileName, 63);
            wchar_t sd[MAX_PATH]; path_join(sd, dir, fd.cFileName);
            path_join(p, sd, L"PARAM.SFO");
            if (!sfo_get(p, "TITLE", s->title, 128)) continue;
            sfo_get(p, "SUB_TITLE", s->sub, 128);
            sfo_get(p, "DETAIL", s->detail, 256);
            s->mtime = fd.ftLastWriteTime;
            path_join(p, sd, L"PARAM.SFO");
            WIN32_FILE_ATTRIBUTE_DATA fa;
            if (GetFileAttributesExW(p, GetFileExInfoStandard, &fa)) s->mtime = fa.ftLastWriteTime;
            s->size = dir_size(sd);
            path_join(p, sd, L"ICON0.PNG");
            if (file_exists(p)) s->icon = load_image_copy(p);
            g.nslots++;
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    /* newest first */
    for (int i = 1; i < g.nslots; i++)
        for (int j = i; j > 0 && CompareFileTime(&g.slots[j].mtime, &g.slots[j - 1].mtime) > 0; j--) {
            save_slot t = g.slots[j]; g.slots[j] = g.slots[j - 1]; g.slots[j - 1] = t;
        }
    backups_dir(dir); path_join(pat, dir, L"*");
    h = FindFirstFileW(pat, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || fd.cFileName[0] == L'.' || g.nbackups == MAX_BACKUPS) continue;
            save_backup* b = &g.backups[g.nbackups++];
            wcsncpy(b->name, fd.cFileName, 63); b->name[63] = 0;
            b->mtime = fd.ftCreationTime; b->slots = 0;
            wchar_t bd[MAX_PATH], bp[MAX_PATH]; path_join(bd, dir, fd.cFileName); path_join(bp, bd, L"*");
            WIN32_FIND_DATAW f2; HANDLE h2 = FindFirstFileW(bp, &f2);
            if (h2 != INVALID_HANDLE_VALUE) {
                do if ((f2.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && f2.cFileName[0] != L'.') b->slots++;
                while (FindNextFileW(h2, &f2));
                FindClose(h2);
            }
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    for (int i = 1; i < g.nbackups; i++)          /* names are dated: sort descending */
        for (int j = i; j > 0 && wcscmp(g.backups[j].name + (g.backups[j].name[0] == L'a' ? 5 : 0),
                                         g.backups[j - 1].name + (g.backups[j - 1].name[0] == L'a' ? 5 : 0)) > 0; j--) {
            save_backup t = g.backups[j]; g.backups[j] = g.backups[j - 1]; g.backups[j - 1] = t;
        }
}

/* Shell copy / recycle: double-NUL-terminated paths, no UI. */
static int shell_op(UINT op, const wchar_t* from, const wchar_t* to, int recycle)
{
    static wchar_t a[MAX_PATH + 2], b[MAX_PATH + 2];
    memset(a, 0, sizeof a); memset(b, 0, sizeof b);
    wcsncpy(a, from, MAX_PATH);
    if (to) wcsncpy(b, to, MAX_PATH);
    SHFILEOPSTRUCTW op_ = {0};
    op_.hwnd = g.hwnd; op_.wFunc = op; op_.pFrom = a; op_.pTo = to ? b : NULL;
    op_.fFlags = FOF_NOCONFIRMATION | FOF_NOCONFIRMMKDIR | FOF_NOERRORUI | FOF_SILENT | (recycle ? FOF_ALLOWUNDO : 0);
    return SHFileOperationW(&op_) == 0 && !op_.fAnyOperationsAborted;
}
static void stamp(wchar_t* out, int cap, const wchar_t* prefix)
{
    SYSTEMTIME st; GetLocalTime(&st);
    _snwprintf(out, cap, L"%s%04d-%02d-%02d_%02d%02d%02d", prefix, st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
}
/* Copy one slot (or all, slot < 0) to a new dated backup folder. */
static int backup_slots(int slot, const wchar_t* prefix)
{
    wchar_t bd[MAX_PATH], name[64], dst[MAX_PATH], src[MAX_PATH], sd[MAX_PATH];
    backups_dir(bd); CreateDirectoryW(g.work, NULL); CreateDirectoryW(bd, NULL);
    stamp(name, 64, prefix); path_join(dst, bd, name); CreateDirectoryW(dst, NULL);
    savedata_dir(sd);
    int ok = 1, n = 0;
    for (int i = 0; i < g.nslots; i++) {
        if (slot >= 0 && i != slot) continue;
        path_join(src, sd, g.slots[i].dir);
        ok &= shell_op(FO_COPY, src, dst, 0); n++;
    }
    if (!n) RemoveDirectoryW(dst);
    return ok && n;
}
/* Keep the newest 10 automatic backups. */
static void prune_auto_backups(void)
{
    scan_saves();
    wchar_t bd[MAX_PATH], p[MAX_PATH]; backups_dir(bd);
    int kept = 0;
    for (int i = 0; i < g.nbackups; i++) {
        if (wcsncmp(g.backups[i].name, L"auto-", 5)) continue;
        if (++kept > 10) { path_join(p, bd, g.backups[i].name); shell_op(FO_DELETE, p, NULL, 1); }
    }
}

static void log_path(wchar_t* out) { path_join(out, g.work, L"logs\\play.err"); }

/* The end of the log (512 KB at most, from a line start) into the log box. */
static void refresh_log(int force)
{
    if (!g.ed_log) return;
    wchar_t p[MAX_PATH]; log_path(p);
    WIN32_FILE_ATTRIBUTE_DATA fa;
    if (!GetFileAttributesExW(p, GetFileExInfoStandard, &fa)) {
        SetWindowTextW(g.ed_log, L"Nenhum log ainda: ele aparece aqui depois da primeira partida iniciada pelo launcher.");
        return;
    }
    const ULONGLONG size = ((ULONGLONG)fa.nFileSizeHigh << 32) | fa.nFileSizeLow;
    if (!force && size == g.log_size && !CompareFileTime(&fa.ftLastWriteTime, &g.log_time)) return;
    g.log_size = size; g.log_time = fa.ftLastWriteTime;
    HANDLE f = CreateFileW(p, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return;
    const DWORD want = size > 512 * 1024 ? 512 * 1024 : (DWORD)size;
    LARGE_INTEGER off; off.QuadPart = (LONGLONG)(size - want);
    SetFilePointerEx(f, off, NULL, FILE_BEGIN);
    char* buf = (char*)malloc(want + 1); DWORD n = 0;
    ReadFile(f, buf, want, &n, NULL); CloseHandle(f); buf[n] = 0;
    char* start = buf;
    if (size > want) { char* nl = strchr(buf, '\n'); if (nl) start = nl + 1; }
    /* filter, and LF -> CRLF for the edit box */
    char* out = (char*)malloc((size_t)n * 2 + 2); size_t o = 0;
    const int only_warn = s_set[S_LOGFILTER].value;
    for (char* l = start; *l; ) {
        char* e = strchr(l, '\n'); const size_t len = e ? (size_t)(e - l) : strlen(l);
        int keep = 1;
        if (only_warn) {
            char tmp[512]; const size_t k = len < 511 ? len : 511;
            for (size_t i = 0; i < k; i++) tmp[i] = (char)tolower((unsigned char)l[i]);
            tmp[k] = 0;
            keep = strstr(tmp, "warn") || strstr(tmp, "error") || strstr(tmp, "fail") || strstr(tmp, "fatal") ||
                   strstr(tmp, "crash") || strstr(tmp, "exception") || strstr(tmp, "erro");
        }
        if (keep) { size_t k = len; if (k && l[k - 1] == '\r') k--; memcpy(out + o, l, k); o += k; out[o++] = '\r'; out[o++] = '\n'; }
        if (!e) break;
        l = e + 1;
    }
    out[o] = 0;
    const int wn = MultiByteToWideChar(CP_UTF8, 0, out, (int)o + 1, NULL, 0);
    wchar_t* w = (wchar_t*)malloc(sizeof(wchar_t) * (size_t)(wn > 0 ? wn : 1));
    if (wn <= 0 || !MultiByteToWideChar(CP_UTF8, 0, out, (int)o + 1, w, wn)) MultiByteToWideChar(CP_ACP, 0, out, -1, w, wn > 0 ? wn : 1);
    SetWindowTextW(g.ed_log, w);
    SendMessageW(g.ed_log, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
    SendMessageW(g.ed_log, EM_SCROLLCARET, 0, 0);
    free(w); free(out); free(buf);
}

/* ---- the command and its environment ------------------------------------ */
static int sharpen_tenths(void) { return s_set[S_SHARPEN].value; }   /* percent */
static void build_command(wchar_t* cmd, int cap, int pick_rpcs3)
{
    int n = _snwprintf(cmd, cap, L"powershell.exe -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File \"%s\\scripts\\windows\\play.ps1\" -Resolution %s",
                       g.root, kResArg[s_set[S_RES].value]);
    if (pick_rpcs3 && g.rpcs3[0]) n += _snwprintf(cmd + n, cap - n, L" -Rpcs3 \"%s\"", g.rpcs3);
    if (s_set[S_SCALE].value) n += _snwprintf(cmd + n, cap - n, L" -Scale %d", s_set[S_SCALE].value);
    if (s_set[S_FPS].value == 0) n += _snwprintf(cmd + n, cap - n, L" -Fps30");
    else if (s_set[S_FPS].value == 2) n += _snwprintf(cmd + n, cap - n, L" -Uncapped");
    if (s_set[S_MODE].value) n += _snwprintf(cmd + n, cap - n, L" -Fullscreen");
    if (s_set[S_FSR].value) n += _snwprintf(cmd + n, cap - n, L" -Fsr");
    if (s_set[S_FXAA].value) n += _snwprintf(cmd + n, cap - n, L" -Fxaa");
    if (s_set[S_FRAMEGEN].value) n += _snwprintf(cmd + n, cap - n, L" -FrameGen");
    const int sh = sharpen_tenths();
    if (sh) n += _snwprintf(cmd + n, cap - n, L" -Sharpen %d.%02d", sh / 100, sh % 100);
}
/* The variables play.ps1 passes on. An option turned off also clears a value
 * the launcher itself inherited. Order matters only for the text shown. */
typedef struct { const wchar_t* name; wchar_t value[MAX_PATH]; } env_pair;
static int build_env(env_pair* e, int cap)
{
    int n = 0;
    #define ENV(nm, val) do { if (n < cap) { e[n].name = nm; if (val) wcsncpy(e[n].value, val, MAX_PATH - 1); else e[n].value[0] = 0; e[n].value[MAX_PATH - 1] = 0; n++; } } while (0)
    wchar_t sh[16]; const int pct = sharpen_tenths();
    if (pct) _snwprintf(sh, 16, L"%d.%02d", pct / 100, pct % 100); else wcscpy(sh, s_set[S_FSR].value ? L"0.8" : L"0");
    ENV(L"LD_FSR", s_set[S_FSR].value ? L"1" : L"0");
    ENV(L"LD_FXAA", s_set[S_FXAA].value ? L"1" : L"0");
    ENV(L"LD_FRAMEGEN", s_set[S_FRAMEGEN].value ? L"1" : L"0");
    ENV(L"LD_SHARPEN", sh);
    { static const wchar_t* const smooth[] = {NULL, L"2", L"4", L"8"};
      static const wchar_t* const dtmax[] = {NULL, L"33", L"25"};
      ENV(L"DES_FPS_DT_SMOOTH", smooth[s_set[S_SMOOTH].value]);
      ENV(L"DES_FPS_DT_MAX_MS", dtmax[s_set[S_DTMAX].value]); }
    ENV(L"DES_VSYNC", s_set[S_VSYNC].value ? L"1" : L"0");
    ENV(L"SAIL_HOST_PLAY", s_set[S_MOVIES].value ? L"1" : L"0");
    ENV(L"DES_SKIP_INTRO", s_set[S_SKIPINTRO].value ? L"1" : NULL);
    ENV(L"DES_AUTO_CONTINUE", s_set[S_CONTINUE].value ? L"1" : NULL);
    ENV(L"PAD_NO_KEYBOARD", s_set[S_KEYBOARD].value ? NULL : L"1");
    ENV(L"PAD_KEYMAP", g.keymap);
    ENV(L"DES_FULLSCREEN", NULL); ENV(L"LD_RES_SCALE", NULL); ENV(L"DES_WINDOW", NULL);
    /* extra variables, one NAME=value per line ('#' comments) */
    FILE* f = _wfopen(g.envfile, L"r, ccs=UTF-8");
    if (f) {
        static wchar_t names[32][128];
        wchar_t line[1024]; int k = 0;
        while (fgetws(line, 1024, f) && k < 32) {
            wchar_t* s = line; while (*s == L' ' || *s == L'\t') s++;
            s[wcscspn(s, L"\r\n")] = 0;
            wchar_t* eq = wcschr(s, L'=');
            if (*s == L'#' || !eq || eq == s) continue;
            *eq = 0;
            wcsncpy(names[k], s, 127); names[k][127] = 0;
            ENV(names[k], eq + 1);
            k++;
        }
        fclose(f);
    }
    #undef ENV
    return n;
}
static void refresh_cmd_box(void)
{
    if (!g.ed_cmd) return;
    static wchar_t cmd[4096], text[8192];
    build_command(cmd, 4096, 0);
    env_pair e[48]; const int n = build_env(e, 48);
    int k = 0;
    for (int i = 0; i < n; i++)
        k += _snwprintf(text + k, 8192 - k, e[i].value[0] ? L"$env:%s = \"%s\"\r\n" : L"Remove-Item Env:%s -ErrorAction Ignore\r\n", e[i].name, e[i].value);
    _snwprintf(text + k, 8192 - k, L"%s", cmd);
    SetWindowTextW(g.ed_cmd, text);
}

/* ---- drawing primitives -------------------------------------------------- */
static void fill(GpGraphics* gr, ARGB c, REAL x, REAL y, REAL w, REAL h)
{ GpBrush* b; GdipCreateSolidFill(c, &b); GdipFillRectangle(gr, b, x, y, w, h); GdipDeleteBrush(b); }
static void gradient(GpGraphics* gr, REAL x0, REAL y0, REAL x1, REAL y1, ARGB c0, ARGB c1, REAL x, REAL y, REAL w, REAL h)
{
    const GpPointF a = {x0, y0}, b = {x1, y1}; GpBrush* br;
    if (GdipCreateLineBrush(&a, &b, c0, c1, WrapTileFlipXY, &br) == 0) { GdipFillRectangle(gr, br, x, y, w, h); GdipDeleteBrush(br); }
}
static void line(GpGraphics* gr, ARGB c, REAL width, REAL x0, REAL y0, REAL x1, REAL y1)
{ GpPen* p; GdipCreatePen1(c, width, UnitPixel, &p); GdipDrawLine(gr, p, x0, y0, x1, y1); GdipDeletePen(p); }
static GpPath* rpath(REAL x, REAL y, REAL w, REAL h, REAL r)
{
    GpPath* p; GdipCreatePath(0, &p);
    const REAL d = r * 2;
    GdipAddPathArc(p, x, y, d, d, 180, 90);
    GdipAddPathArc(p, x + w - d, y, d, d, 270, 90);
    GdipAddPathArc(p, x + w - d, y + h - d, d, d, 0, 90);
    GdipAddPathArc(p, x, y + h - d, d, d, 90, 90);
    GdipClosePathFigure(p);
    return p;
}
/* Rounded rectangle in logical units: fill (0 = none) and border (0 = none). */
static void rrect(GpGraphics* gr, REAL x, REAL y, REAL w, REAL h, REAL r, ARGB fillc, ARGB border, REAL bw)
{
    GpPath* p = rpath(S(x), S(y), S(w), S(h), S(r));
    if (fillc) { GpBrush* b; GdipCreateSolidFill(fillc, &b); GdipFillPath(gr, b, p); GdipDeleteBrush(b); }
    if (border) { GpPen* pen; GdipCreatePen1(border, S(bw), UnitPixel, &pen); GdipDrawPath(gr, pen, p); GdipDeletePen(pen); }
    GdipDeletePath(p);
}
static void circle(GpGraphics* gr, ARGB c, REAL cx, REAL cy, REAL r)
{ GpBrush* b; GdipCreateSolidFill(c, &b); GdipFillEllipse(gr, b, S(cx - r), S(cy - r), S(2 * r), S(2 * r)); GdipDeleteBrush(b); }
/* Text in logical units. */
static void text(GpGraphics* gr, const wchar_t* t, GpFont* f, ARGB c, GpStringFormat* fmt, REAL x, REAL y, REAL w, REAL h)
{
    GpBrush* b; GdipCreateSolidFill(c, &b);
    const GpRectF r = {S(x), S(y), S(w), S(h)};
    GdipDrawString(gr, t, -1, f, &r, fmt, b);
    GdipDeleteBrush(b);
}
/* Height in logical units of wrapped text. */
static REAL text_h(const wchar_t* t, GpFont* f, REAL w)
{
    if (!t || !*t) return 0;
    const GpRectF lay = {0, 0, S(w), S(4000)}; GpRectF box = {0};
    GdipMeasureString(g.measure, t, -1, f, &lay, g.fmt_wrap, &box, NULL, NULL);
    return box.Height / g.s;
}
static REAL text_w(const wchar_t* t, GpFont* f)
{
    const GpRectF lay = {0, 0, S(4000), S(200)}; GpRectF box = {0};
    GdipMeasureString(g.measure, t, -1, f, &lay, g.fmt_left, &box, NULL, NULL);
    return box.Width / g.s;
}
static void chevron_down(GpGraphics* gr, ARGB c, REAL cx, REAL cy)
{
    line(gr, c, S(1.6f), S(cx - 5), S(cy - 2.5f), S(cx), S(cy + 2.5f));
    line(gr, c, S(1.6f), S(cx), S(cy + 2.5f), S(cx + 5), S(cy - 2.5f));
}
static void quality(GpGraphics* gr)
{
    GdipSetSmoothingMode(gr, SmoothAntiAlias);
    GdipSetTextRenderingHint(gr, TextAntiAliasGridFit);
    GdipSetInterpolationMode(gr, InterpHighQualityBicubic);
    GdipSetPixelOffsetMode(gr, PixelOffsetHighQuality);
}

/* ---- regions ------------------------------------------------------------- */
static REAL s_clip_y0 = 0, s_clip_y1 = WIN_H;
static void reg(REAL x, REAL y, REAL w, REAL h, int id)
{
    if (g.nreg >= (int)(sizeof g.reg / sizeof g.reg[0])) return;
    if (y + h <= s_clip_y0 || y >= s_clip_y1) return;          /* scrolled out */
    if (y < s_clip_y0) { h -= s_clip_y0 - y; y = s_clip_y0; }
    if (y + h > s_clip_y1) h = s_clip_y1 - y;
    region* r = &g.reg[g.nreg++]; r->x = x; r->y = y; r->w = w; r->h = h; r->id = id;
}
static int hit_test(int px, int py)
{
    const REAL x = px / g.s, y = py / g.s;
    for (int i = g.nreg - 1; i >= 0; i--) {
        const region* r = &g.reg[i];
        if (x >= r->x && x < r->x + r->w && y >= r->y && y < r->y + r->h) return r->id;
    }
    return 0;
}
static const region* find_region(int id)
{
    for (int i = g.nreg - 1; i >= 0; i--) if (g.reg[i].id == id) return &g.reg[i];
    return NULL;
}

/* ---- widgets ------------------------------------------------------------- */
static int hovered(int id) { return g.hover == id; }

static void w_button(GpGraphics* gr, REAL x, REAL y, REAL w, REAL h, const wchar_t* label, int action, int primary)
{
    const int id = RID(R_ACTION, action), on = hovered(id);
    if (primary) rrect(gr, x, y, w, h, 6, on ? 0xFF3A2F1E : 0xFF2A2318, kGold, 1);
    else rrect(gr, x, y, w, h, 6, on ? 0xFF221E19 : 0xFF181512, on ? 0xFF6E5F45 : kBorder, 1);
    text(gr, label, g.f_small, primary ? kGold : (on ? kText : 0xFFC9C2B4), g.fmt_center, x, y, w, h);
    reg(x, y, w, h, id);
}
static void w_link(GpGraphics* gr, REAL x, REAL y, const wchar_t* label, int action, REAL* out_w)
{
    const int id = RID(R_ACTION, action);
    const REAL w = text_w(label, g.f_small) + 4;
    text(gr, label, g.f_small, hovered(id) ? 0xFFE6CC94 : kGold, g.fmt_left, x, y, w + 8, 22);
    if (hovered(id)) line(gr, 0x80E6CC94, S(1), S(x + 2), S(y + 19), S(x + w - 2), S(y + 19));
    reg(x, y, w, 22, id);
    if (out_w) *out_w = w;
}
static void w_toggle(GpGraphics* gr, REAL x, REAL y, int si)
{
    const int on = s_set[si].value, id = RID(R_TOGGLE, si), hv = hovered(id);
    rrect(gr, x, y, 42, 24, 12, on ? (hv ? 0xFFD8BC86 : kGold) : (hv ? 0xFF3A342C : 0xFF2A2621), on ? 0 : 0xFF3A342C, 1);
    circle(gr, on ? 0xFF1A150D : 0xFF8E867A, on ? x + 30 : x + 12, y + 12, 8);
    reg(x - 4, y - 4, 50, 32, id);
}
static void w_list(GpGraphics* gr, REAL x, REAL y, REAL w, int si)
{
    const int id = RID(R_LIST, si), hv = hovered(id) || g.drop == si;
    rrect(gr, x, y, w, 38, 6, hv ? 0xFF1E1A16 : 0xFF16130F, g.drop == si ? kGold : (hv ? 0xFF5A4F3E : kBorder), 1);
    text(gr, s_set[si].values[s_set[si].value], g.f_value, kText, g.fmt_ell, x + 12, y, w - 40, 38);
    chevron_down(gr, kGold, S(x + w - 18), S(y + 19));
    reg(x, y, w, 38, id);
}
static void w_slider(GpGraphics* gr, REAL x, REAL y, REAL w, int si)
{
    const int id = RID(R_SLIDER, si), v = s_set[si].value, hv = hovered(id) || g.drag == si;
    const REAL tw = w - 92, t = (REAL)v / (s_set[si].count - 1);
    rrect(gr, x, y + 15, tw, 6, 3, 0xFF2A2621, 0, 0);
    if (v) rrect(gr, x, y + 15, tw * t, 6, 3, kGold, 0, 0);
    circle(gr, hv ? 0xFFE6CC94 : kGold, x + tw * t, y + 18, hv ? 9 : 8);
    wchar_t s[16]; _snwprintf(s, 16, v ? L"%d%%" : L"Desligada", v);
    text(gr, s, g.f_value, v ? kGold : kDim, g.fmt_right, x + tw, y, 92, 36);
    reg(x - 8, y, tw + 16, 36, id);
}

/* A settings row: label and help on the left, the control on the right.
 * Returns the row's height. */
static REAL row_setting(GpGraphics* gr, REAL y, int si)
{
    const setting* st = &s_set[si];
    const REAL tw = PAGE_W - CTRL_W - 60;
    const REAL hh = text_h(st->help, g.f_help, tw);
    const REAL h = 22 + (hh > 0 ? hh + 6 : 0) + 30;
    text(gr, st->label, g.f_label, kText, g.fmt_left, PAGE_X + 4, y + 14, tw, 24);
    if (hh > 0) text(gr, st->help, g.f_help, kDim, g.fmt_wrap, PAGE_X + 4, y + 40, tw, hh + 4);
    const REAL cx = PAGE_X + PAGE_W - CTRL_W, cy = y + h / 2;
    if (st->type == T_LIST) w_list(gr, cx, cy - 19, CTRL_W, si);
    else if (st->type == T_TOGGLE) w_toggle(gr, cx + CTRL_W - 42, cy - 12, si);
    else w_slider(gr, cx, cy - 18, CTRL_W, si);
    line(gr, kLine, S(1), S(PAGE_X), S(y + h), S(PAGE_X + PAGE_W), S(y + h));
    return h;
}
static REAL section(GpGraphics* gr, REAL y, const wchar_t* title)
{
    y += 18;
    text(gr, title, g.f_section, kGold, g.fmt_left, PAGE_X, y, PAGE_W, 28);
    line(gr, kLine, S(1), S(PAGE_X), S(y + 32), S(PAGE_X + PAGE_W), S(y + 32));
    gradient(gr, S(PAGE_X), 0, S(PAGE_X + 70), 0, kGold, 0x00C9AA71, S(PAGE_X), S(y + 31), S(70), S(2));
    return y + 34;
}
static REAL page_header(GpGraphics* gr, REAL y, const wchar_t* title, const wchar_t* sub)
{
    text(gr, title, g.f_page, kText, g.fmt_left, PAGE_X, y + 28, PAGE_W - 220, 50);
    const wchar_t* chip = L"Vale na próxima partida";
    const REAL cw = text_w(chip, g.f_tiny) + 26;
    rrect(gr, PAGE_X + PAGE_W - cw, y + 40, cw, 26, 13, 0, g.saved_flash ? 0xFF8C7A55 : kBorder, 1);
    text(gr, chip, g.f_tiny, kDim, g.fmt_center, PAGE_X + PAGE_W - cw, y + 40, cw, 26);
    const REAL sh = text_h(sub, g.f_help, PAGE_W - 120);
    text(gr, sub, g.f_help, kDim, g.fmt_wrap, PAGE_X, y + 84, PAGE_W - 120, sh + 4);
    return y + 84 + sh + 10;
}

/* ---- pages ---------------------------------------------------------------- */
static void card(GpGraphics* gr, REAL x, REAL y, REAL w, REAL h, const wchar_t* title)
{
    rrect(gr, x, y, w, h, 10, kCard, kBorder, 1);
    text(gr, title, g.f_section, kGold, g.fmt_left, x + 20, y + 16, w - 40, 26);
}
static int game_ready(const wchar_t** why)
{
    wchar_t p[MAX_PATH];
    path_join(p, g.root, L"build\\MyGameRecomp.exe");
    if (!file_exists(p)) { *why = L"Falta compilar"; return 0; }
    path_join(p, g.work, L"disc\\PS3_GAME\\USRDIR\\EBOOT.elf");
    if (!file_exists(p)) { *why = L"Falta o disco"; return 0; }
    *why = L"Pronto";
    return 1;
}
static void kv(GpGraphics* gr, REAL x, REAL y, REAL w, const wchar_t* k, const wchar_t* v, ARGB vc)
{
    text(gr, k, g.f_small, kDim, g.fmt_left, x, y, 90, 22);
    text(gr, v, g.f_small, vc, g.fmt_ell, x + 92, y, w - 92, 22);
}

static REAL page_home(GpGraphics* gr, REAL y0)
{
    const REAL hx = SIDE_W, hw = WIN_W - SIDE_W, hh = 360;
    /* hero art: PIC1 on the right, fading into the page */
    if (g.hero) {
        UINT iw = 0, ih = 0; GdipGetImageWidth(g.hero, &iw); GdipGetImageHeight(g.hero, &ih);
        const REAL dw = hw * 0.78f, dh = hh;
        /* crop the art to the hero's aspect: full width, from the top */
        REAL sw = (REAL)iw, sh = sw * dh / dw;
        if (sh > ih) { sh = (REAL)ih; sw = sh * dw / dh; }
        if (iw && ih)
            GdipDrawImageRectRectI(gr, g.hero, (INT)S(hx + hw - dw), (INT)S(y0), (INT)S(dw), (INT)S(dh),
                                   (INT)((iw - sw) / 2), 0, (INT)sw, (INT)sh, UnitPixel, NULL, NULL, NULL);
        gradient(gr, S(hx + hw - dw), 0, S(hx + hw - dw * 0.35f), 0, 0xFF0B0A09, 0x000B0A09, S(hx + hw - dw), S(y0), S(dw * 0.65f), S(dh));
        gradient(gr, 0, S(y0 + dh * 0.55f), 0, S(y0 + dh), 0x000B0A09, 0xFF0B0A09, S(hx), S(y0 + dh * 0.55f), S(hw), S(dh * 0.45f));
        gradient(gr, 0, S(y0), 0, S(y0 + 80), 0x900B0A09, 0x000B0A09, S(hx), S(y0), S(hw), S(80));
    }
    wchar_t s[160];
    _snwprintf(s, 160, L"%s  ·  VERSÃO %s", g.title_id, g.app_ver[0] ? g.app_ver : L"—");
    text(gr, s, g.f_section, kGold, g.fmt_left, PAGE_X + 4, y0 + 92, 600, 26);
    text(gr, L"DEMON'S SOULS", g.f_hero, kText, g.fmt_left, PAGE_X - 2, y0 + 116, 760, 84);
    _snwprintf(s, 160, L"Compilado em %s%s%s", g.built[0] ? g.built : L"—", g.commit[0] ? L"  ·  port " : L"", g.commit);
    text(gr, s, g.f_small, kDim, g.fmt_left, PAGE_X + 4, y0 + 198, 700, 22);
    /* JOGAR / PARAR */
    const int running = g.game != NULL;
    const REAL bx = PAGE_X + 4, by = y0 + 240, bw = 210, bh = 62;
    const int id = RID(R_ACTION, running ? A_STOP : A_PLAY), hv = hovered(id);
    const ARGB rgb = running ? 0x00E0874A : 0x00C9AA71;
    for (int i = 3; i >= 1; i--)                    /* soft glow: three widening rings */
        rrect(gr, bx - i * 4, by - i * 4, bw + i * 8, bh + i * 8, 8 + i * 4, rgb | ((ARGB)((hv ? 0x26 : 0x16) - i * 4) << 24), 0, 0);
    {
        GpPath* p = rpath(S(bx), S(by), S(bw), S(bh), S(8));
        const GpPointF a = {0, S(by)}, b = {0, S(by + bh)}; GpBrush* br;
        const ARGB c0 = running ? (hv ? 0xFFF09A5C : 0xFFE0874A) : (hv ? 0xFFEBD29A : 0xFFDCC08A);
        const ARGB c1 = running ? 0xFFB8612C : (hv ? 0xFFB89752 : 0xFFA88848);
        GdipCreateLineBrush(&a, &b, c0, c1, WrapTileFlipXY, &br); GdipFillPath(gr, br, p); GdipDeleteBrush(br);
        GdipDeletePath(p);
    }
    text(gr, running ? L"■   PARAR" : L"▶   JOGAR", g.f_button, 0xFF17120A, g.fmt_center, bx, by, bw, bh);
    reg(bx, by, bw, bh, id);
    if (running) text(gr, L"O jogo está rodando. As mudanças valem na próxima partida.", g.f_small, kOrange, g.fmt_left, bx + bw + 24, by + 20, 520, 22);

    /* cards */
    const REAL cy = y0 + hh + 8, cw = (PAGE_W - 32) / 3, ch = 300;
    REAL x = PAGE_X;
    card(gr, x, cy, cw, ch, L"AJUSTES RÁPIDOS");
    {
        static const int quick[] = {S_MODE, S_FPS, S_FSR, S_FRAMEGEN};
        REAL ry = cy + 56;
        for (int i = 0; i < 4; i++) {
            const int si = quick[i];
            text(gr, s_set[si].label, g.f_value, kText, g.fmt_ell, x + 20, ry, cw - (s_set[si].type == T_LIST ? 190 : 90), 38);
            if (s_set[si].type == T_LIST) w_list(gr, x + cw - 20 - 150, ry, 150, si);
            else w_toggle(gr, x + cw - 20 - 42, ry + 7, si);
            if (i < 3) line(gr, kLine, S(1), S(x + 20), S(ry + 46), S(x + cw - 20), S(ry + 46));
            ry += 52;
        }
        w_link(gr, x + 20, cy + ch - 40, L"Todas as opções de gráficos  →", A_ALLGFX, NULL);
    }
    x += cw + 16;
    card(gr, x, cy, cw, ch, L"ÚLTIMO SAVE");
    if (g.nslots) {
        const save_slot* sl = &g.slots[0];
        REAL tx = x + 20;
        if (sl->icon) { GdipDrawImageRectI(gr, sl->icon, (INT)S(x + 20), (INT)S(cy + 56), (INT)S(96), (INT)S(53)); tx = x + 20; }
        const REAL ty = cy + 56 + (sl->icon ? 64 : 0);
        text(gr, sl->sub[0] ? sl->sub : sl->title, g.f_label, kText, g.fmt_ell, tx, ty, cw - 40, 26);
        const REAL dh = text_h(sl->detail, g.f_small, cw - 40);
        text(gr, sl->detail, g.f_small, kDim, g.fmt_wrap, tx, ty + 28, cw - 40, dh > 66 ? 66 : dh + 2);
        wchar_t t[64]; fmt_time(&sl->mtime, t, 64);
        text(gr, t, g.f_tiny, kFaint, g.fmt_left, tx, ty + 30 + (dh > 66 ? 66 : dh), cw - 40, 20);
    } else {
        text(gr, L"Nenhum save encontrado ainda.", g.f_small, kDim, g.fmt_wrap, x + 20, cy + 60, cw - 40, 44);
    }
    { REAL lw = 0; w_link(gr, x + 20, cy + ch - 40, L"Gerenciar saves", A_MANAGE_SAVES, &lw);
      w_link(gr, x + 20 + lw + 18, cy + ch - 40, L"Abrir pasta", A_OPEN_SAVES, NULL); }
    x += cw + 16;
    card(gr, x, cy, cw, ch, L"STATUS");
    {
        const wchar_t* why; const int ok = game_ready(&why);
        kv(gr, x + 20, cy + 58, cw - 40, L"Jogo", running ? L"Rodando" : why, running ? kOrange : ok ? kGreen : kRed);
        kv(gr, x + 20, cy + 84, cw - 40, L"Versão", g.title_id, kText);
        kv(gr, x + 20, cy + 110, cw - 40, L"Compilado", g.built[0] ? g.built : L"—", kText);
        kv(gr, x + 20, cy + 136, cw - 40, L"Port", g.commit[0] ? g.commit : L"—", kText);
        text(gr, L"Pasta", g.f_small, kDim, g.fmt_left, x + 20, cy + 162, 90, 22);
        const REAL ph = text_h(g.root, g.f_small, cw - 132);
        text(gr, g.root, g.f_small, kText, g.fmt_wrap, x + 112, cy + 162, cw - 132, ph + 2);
        REAL lw = 0; w_link(gr, x + 20, cy + ch - 40, L"Ver o log", A_VIEW_LOG, &lw);
        w_link(gr, x + 20 + lw + 18, cy + ch - 40, L"Abrir o log", A_OPEN_LOG, NULL);
    }
    return cy + ch + 30;
}

static REAL page_gfx(GpGraphics* gr, REAL y)
{
    y = page_header(gr, y, L"GRÁFICOS", L"Como ficam a janela e a imagem do jogo. O jogo desenha em 1280×720 multiplicado pela resolução interna; estas opções moldam o que chega à tela.");
    y = section(gr, y, L"TELA");
    y += row_setting(gr, y, S_MODE);
    y += row_setting(gr, y, S_RES);
    y += row_setting(gr, y, S_SCALE);
    y = section(gr, y + 10, L"QUALIDADE DE IMAGEM");
    y += row_setting(gr, y, S_FSR);
    y += row_setting(gr, y, S_FXAA);
    y += row_setting(gr, y, S_SHARPEN);
    return y + 30;
}
static REAL page_perf(GpGraphics* gr, REAL y)
{
    y = page_header(gr, y, L"DESEMPENHO", L"Ritmo dos quadros. O jogo é limitado pela CPU; a GPU costuma sobrar, e é ela que faz o frame generation.");
    y = section(gr, y, L"RITMO DOS QUADROS");
    y += row_setting(gr, y, S_FPS);
    y += row_setting(gr, y, S_VSYNC);
    y = section(gr, y + 10, L"MOVIMENTO NO MODO 60");
    y += row_setting(gr, y, S_SMOOTH);
    y += row_setting(gr, y, S_DTMAX);
    y = section(gr, y + 10, L"GERAÇÃO DE QUADROS");
    y += row_setting(gr, y, S_FRAMEGEN);
    y += 16;
    text(gr, L"No jogo, F8 abre o menu de opções do port (taxa de quadros e sincronização mudam na hora).",
         g.f_help, kFaint, g.fmt_wrap, PAGE_X, y, PAGE_W, 40);
    return y + 60;
}
static REAL page_game(GpGraphics* gr, REAL y)
{
    y = page_header(gr, y, L"JOGO", L"Abertura, cinemáticas, o firmware e os saves.");
    y = section(gr, y, L"PARTIDA");
    y += row_setting(gr, y, S_MOVIES);
    y += row_setting(gr, y, S_SKIPINTRO);
    y += row_setting(gr, y, S_CONTINUE);
    /* RPCS3 folder */
    {
        const REAL tw = PAGE_W - CTRL_W - 60;
        text(gr, L"Pasta do RPCS3", g.f_label, kText, g.fmt_left, PAGE_X + 4, y + 14, tw, 24);
        text(gr, g.rpcs3[0] ? g.rpcs3 : L"Ainda não escolhida: é pedida na primeira partida. O dev_flash dela traz o codec de áudio (música e vozes).",
             g.f_help, g.rpcs3[0] ? kDim : kOrange, g.fmt_wrap, PAGE_X + 4, y + 40, tw, 44);
        w_button(gr, PAGE_X + PAGE_W - 150, y + 24, 150, 38, L"Escolher…", A_RPCS3, 0);
        y += 90;
        line(gr, kLine, S(1), S(PAGE_X), S(y), S(PAGE_X + PAGE_W), S(y));
    }
    y = section(gr, y + 10, L"SAVES");
    y += row_setting(gr, y, S_AUTOBACKUP);
    y += 14;
    w_button(gr, PAGE_X, y, 170, 36, L"Backup de tudo", A_BACKUP_ALL, 1);
    w_button(gr, PAGE_X + 184, y, 150, 36, L"Abrir pasta", A_OPEN_SAVES, 0);
    w_button(gr, PAGE_X + 348, y, 190, 36, L"Abrir pasta de backups", A_OPEN_BACKUPS, 0);
    y += 52;
    if (!g.nslots) {
        text(gr, L"Nenhum save em work\\gamedata. Eles aparecem aqui depois que o jogo salvar.", g.f_help, kDim, g.fmt_left, PAGE_X, y, PAGE_W, 24);
        y += 34;
    }
    for (int i = 0; i < g.nslots; i++) {
        const save_slot* sl = &g.slots[i];
        rrect(gr, PAGE_X, y, PAGE_W, 76, 8, kCard, kBorder, 1);
        if (sl->icon) GdipDrawImageRectI(gr, sl->icon, (INT)S(PAGE_X + 12), (INT)S(y + 11), (INT)S(98), (INT)S(54));
        else rrect(gr, PAGE_X + 12, y + 11, 98, 54, 4, 0xFF1E1A16, 0, 0);
        const REAL tx = PAGE_X + 126, tw = PAGE_W - 126 - 260;
        text(gr, sl->sub[0] ? sl->sub : sl->title, g.f_value, kText, g.fmt_ell, tx, y + 10, tw, 24);
        text(gr, sl->detail, g.f_small, kDim, g.fmt_ell, tx, y + 32, tw, 20);
        wchar_t t[64], m[160]; fmt_time(&sl->mtime, t, 64);
        _snwprintf(m, 160, L"%s  ·  %s  ·  %.1f MB", sl->dir, t, sl->size / 1048576.0);
        text(gr, m, g.f_tiny, kFaint, g.fmt_ell, tx, y + 52, tw, 18);
        w_button(gr, PAGE_X + PAGE_W - 244, y + 20, 110, 36, L"Backup", A_SLOT_BACKUP + i, 0);
        w_button(gr, PAGE_X + PAGE_W - 122, y + 20, 110, 36, L"Apagar…", A_SLOT_DELETE + i, 0);
        y += 86;
    }
    y = section(gr, y + 6, L"BACKUPS");
    if (!g.nbackups) { text(gr, L"Nenhum backup ainda.", g.f_help, kDim, g.fmt_left, PAGE_X, y + 8, PAGE_W, 24); y += 40; }
    for (int i = 0; i < g.nbackups; i++) {
        const save_backup* b = &g.backups[i];
        wchar_t t[64], m[200]; fmt_time(&b->mtime, t, 64);
        const int autob = !wcsncmp(b->name, L"auto-", 5);
        _snwprintf(m, 200, L"%s  ·  %d save%s%s", t, b->slots, b->slots == 1 ? L"" : L"s", autob ? L"  ·  automático" : L"");
        text(gr, m, g.f_value, kText, g.fmt_left, PAGE_X + 4, y + 10, PAGE_W - 280, 26);
        text(gr, b->name, g.f_tiny, kFaint, g.fmt_left, PAGE_X + 4, y + 34, PAGE_W - 280, 18);
        w_button(gr, PAGE_X + PAGE_W - 244, y + 12, 110, 34, L"Restaurar…", A_BK_RESTORE + i, 0);
        w_button(gr, PAGE_X + PAGE_W - 122, y + 12, 110, 34, L"Apagar…", A_BK_DELETE + i, 0);
        line(gr, kLine, S(1), S(PAGE_X), S(y + 58), S(PAGE_X + PAGE_W), S(y + 58));
        y += 60;
    }
    return y + 30;
}

static int pad_state(XINPUT_STATE* st)
{
    if (!g.xget) return 0;
    for (DWORD i = 0; i < 4; i++) if (g.xget(i, st) == ERROR_SUCCESS) return 1;
    return 0;
}
static int xs_down(const XINPUT_STATE* st, int xs)
{
    if (xs == 6) return st->Gamepad.bLeftTrigger > 30;
    if (xs == 7) return st->Gamepad.bRightTrigger > 30;
    return xs >= 0 && (st->Gamepad.wButtons & k_xs_bit[xs]) != 0;
}
static REAL page_ctrl(GpGraphics* gr, REAL y)
{
    y = page_header(gr, y, L"CONTROLES", L"Teclado, mouse e controle Xbox. Salvo na hora em work\\input.ini, que o jogo lê ao abrir. DualShock 4 e DualSense usam o layout do PS3 direto.");
    y += row_setting(gr, y, S_KEYBOARD);
    y = section(gr, y + 10, L"BOTÕES");
    { REAL lw = text_w(L"Restaurar padrões", g.f_small), pw = text_w(L"Layout PC (WASD + mouse)", g.f_small);
      w_link(gr, PAGE_X + PAGE_W - lw - 6, y - 30, L"Restaurar padrões", A_CTRL_DEFAULTS, NULL);
      w_link(gr, PAGE_X + PAGE_W - lw - pw - 34, y - 30, L"Layout PC (WASD + mouse)", A_CTRL_PC, NULL); }
    text(gr, L"✕ confirma nos menus. Clique numa célula e aperte a tecla, o botão do mouse ou o do controle (Esc cancela, Delete limpa). Aperte qualquer coisa aqui para testar: os controles do PS3 que ela aciona acendem. Esc fecha o jogo e não pode ser usado.",
         g.f_help, kDim, g.fmt_wrap, PAGE_X, y + 6, PAGE_W, 44);
    y += 56;
    const REAL c0 = PAGE_X + 4, c1 = PAGE_X + 300, c2 = PAGE_X + 620, cw = 240;
    text(gr, L"PS3", g.f_tiny, kFaint, g.fmt_left, c0, y, 200, 20);
    text(gr, L"TECLADO / MOUSE", g.f_tiny, kFaint, g.fmt_left, c1, y, 200, 20);
    text(gr, L"CONTROLE XBOX", g.f_tiny, kFaint, g.fmt_left, c2, y, 200, 20);
    y += 24;
    line(gr, kLine, S(1), S(PAGE_X), S(y), S(PAGE_X + PAGE_W), S(y));
    XINPUT_STATE st; memset(&st, 0, sizeof st);
    const int pad = pad_state(&st);
    for (int r = 0; r < KM_KEYS; r++) {
        const int live = (GetForegroundWindow() == g.hwnd && km_held(r)) ||
                         (pad && r < KM_BUTTONS && xs_down(&st, s_km_xs[r]));
        if (live) fill(gr, 0x26C9AA71, S(PAGE_X), S(y + 1), S(PAGE_W), S(39));
        text(gr, k_km_label[r], r < 4 ? g.f_sym : g.f_value, live ? kGold : kText, g.fmt_left, c0, y + 2, 260, 36);
        for (int c = 0; c < 2; c++) {
            const REAL cx = c ? c2 : c1;
            const int id = RID(R_BIND, r * 2 + c), cap = g.cap_row == r && g.cap_col == c;
            wchar_t nm[64];
            if (c == 1 && r >= KM_BUTTONS) { text(gr, r < 20 ? L"Analógico esquerdo" : L"Analógico direito", g.f_small, kFaint, g.fmt_left, cx + 12, y + 2, cw, 36); continue; }
            if (cap) wcscpy(nm, c ? L"Aperte um botão…" : L"Aperte uma tecla…");
            else if (c == 0) {
                key_name(s_km_vk[r][0], nm, 64);
                if (s_km_vk[r][1]) { wchar_t k2[32]; key_name(s_km_vk[r][1], k2, 32); wcsncat(nm, L"  /  ", 63 - wcslen(nm)); wcsncat(nm, k2, 63 - wcslen(nm)); }
            }
            else wcscpy(nm, s_km_xs[r] < 0 ? L"—" : k_xs_label[s_km_xs[r]]);
            const int hv = hovered(id);
            rrect(gr, cx, y + 5, cw, 30, 6, cap ? 0xFF2A2318 : (hv ? 0xFF1E1A16 : 0xFF16130F), cap ? kGold : (hv ? 0xFF5A4F3E : kBorder), 1);
            text(gr, nm, g.f_small, cap ? kGold : kText, g.fmt_ell, cx + 12, y + 5, cw - 24, 30);
            reg(cx, y + 5, cw, 30, id);
        }
        y += 40;
        line(gr, kLine, S(1), S(PAGE_X), S(y), S(PAGE_X + PAGE_W), S(y));
    }
    return y + 30;
}
/* Advanced: fixed layout; the boxes are edit controls placed over it. */
static RECT s_box_log, s_box_env, s_box_cmd;
static REAL page_adv(GpGraphics* gr, REAL y)
{
    y = page_header(gr, y, L"AVANÇADO", L"O log da última partida, variáveis de ambiente extras e o comando que o launcher executa.");
    y = section(gr, y - 6, L"LOG DO JOGO");
    text(gr, L"work\\logs\\play.err", g.f_tiny, kFaint, g.fmt_left, PAGE_X + 140, y - 28, 300, 20);
    { const REAL bx = PAGE_X + PAGE_W - 380;
      text(gr, s_set[S_LOGFILTER].label, g.f_small, kDim, g.fmt_right, bx - 200, y + 8, 190, 24);
      w_toggle(gr, bx, y + 8, S_LOGFILTER);
      w_button(gr, bx + 60, y + 4, 150, 32, L"Copiar tudo", A_LOG_COPY, 0);
      w_button(gr, bx + 220, y + 4, 160, 32, L"Abrir no Bloco", A_OPEN_LOG, 0); }
    y += 46;
    const REAL lh = 240;
    rrect(gr, PAGE_X, y, PAGE_W, lh, 6, 0xFF0F0E0C, kBorder, 1);
    SetRect(&s_box_log, (int)S(PAGE_X + 8), (int)S(y + 6), (int)S(PAGE_X + PAGE_W - 4), (int)S(y + lh - 6));
    y += lh;
    y = section(gr, y, L"VARIÁVEIS EXTRAS");
    text(gr, L"Uma por linha, NOME=valor (# comenta). Valem por cima das opções acima.", g.f_tiny, kFaint, g.fmt_left, PAGE_X + 220, y - 28, 600, 20);
    y += 8;
    rrect(gr, PAGE_X, y, PAGE_W, 88, 6, 0xFF0F0E0C, kBorder, 1);
    SetRect(&s_box_env, (int)S(PAGE_X + 8), (int)S(y + 6), (int)S(PAGE_X + PAGE_W - 4), (int)S(y + 82));
    y += 88;
    y = section(gr, y, L"COMANDO EQUIVALENTE");
    { REAL lw = text_w(L"Restaurar todas as opções", g.f_small);
      w_link(gr, PAGE_X + PAGE_W - lw - 6, y - 30, L"Restaurar todas as opções", A_ALL_DEFAULTS, NULL); }
    y += 8;
    rrect(gr, PAGE_X, y, PAGE_W, 96, 6, 0xFF0F0E0C, kBorder, 1);
    SetRect(&s_box_cmd, (int)S(PAGE_X + 8), (int)S(y + 6), (int)S(PAGE_X + PAGE_W - 4), (int)S(y + 90));
    return y + 110;
}

static void sidebar(GpGraphics* gr)
{
    fill(gr, kSide, 0, 0, S(SIDE_W), S(WIN_H));
    line(gr, kLine, S(1), S(SIDE_W - 0.5f), 0, S(SIDE_W - 0.5f), S(WIN_H));
    text(gr, L"DEMON'S SOULS", g.f_logo, kText, g.fmt_left, 24, 26, SIDE_W - 30, 32);
    text(gr, L"P O R T   P C", g.f_tiny, kGold, g.fmt_left, 26, 58, SIDE_W - 30, 18);
    for (int p = 0; p < P_COUNT; p++) {
        const REAL y = 104 + p * 46;
        const int id = RID(R_NAV, p), on = g.page == p, hv = hovered(id);
        if (on || hv) rrect(gr, 12, y, SIDE_W - 24, 40, 7, on ? 0xFF1F1B16 : 0xFF181512, 0, 0);
        if (on) rrect(gr, 2, y + 8, 4, 24, 2, kGold, 0, 0);
        text(gr, k_page_icon[p], g.f_icon, on ? kGold : kDim, g.fmt_center, 24, y, 26, 40);
        text(gr, k_page_name[p], g.f_nav, on ? kGold : (hv ? kText : 0xFFB0A898), g.fmt_left, 60, y, SIDE_W - 70, 40);
        reg(12, y, SIDE_W - 24, 40, id);
    }
    const REAL by = WIN_H - 168;
    line(gr, kLine, S(1), S(16), S(by), S(SIDE_W - 16), S(by));
    circle(gr, g.saved_flash ? kGreen : 0xFF4F8F53, 26, by + 26, 4);
    text(gr, L"Salvo automaticamente", g.f_small, kText, g.fmt_left, 38, by + 15, SIDE_W - 44, 22);
    text(gr, L"Cada mudança é salva na hora e vale na próxima vez que o jogo abrir.", g.f_tiny, kDim, g.fmt_wrap, 20, by + 42, SIDE_W - 36, 48);
    text(gr, L"Jogar.bat  ·  F8 no jogo: opções", g.f_tiny, kFaint, g.fmt_left, 20, WIN_H - 64, SIDE_W - 30, 18);
    text(gr, L"Launcher 2.0", g.f_tiny, kFaint, g.fmt_left, 20, WIN_H - 40, SIDE_W - 30, 18);
}

static void chrome(GpGraphics* gr)
{
    const REAL y = 10;
    if (g.page == P_HOME) rrect(gr, WIN_W - 98, y - 4, 90, 38, 10, 0x900B0A09, 0, 0);   /* readable over the art */
    for (int k = 0; k < 2; k++) {
        const REAL x = WIN_W - 12 - 40 * (2 - k);
        const int id = RID(R_ACTION, k ? A_CLOSE : A_MIN), hv = hovered(id);
        if (hv) rrect(gr, x, y, 36, 30, 6, k ? 0xC0A33A2E : 0x50FFFFFF, 0, 0);
        const ARGB c = hv ? kText : kDim;
        if (k) { line(gr, c, S(1.4f), S(x + 12), S(y + 9), S(x + 24), S(y + 21)); line(gr, c, S(1.4f), S(x + 24), S(y + 9), S(x + 12), S(y + 21)); }
        else line(gr, c, S(1.4f), S(x + 12), S(y + 15), S(x + 24), S(y + 15));
        reg(x, y, 36, 30, id);
    }
}

static void dropdown(GpGraphics* gr)
{
    if (g.drop < 0) return;
    const region* a = find_region(RID(R_LIST, g.drop));
    if (!a) { g.drop = -1; return; }
    const setting* st = &s_set[g.drop];
    const REAL ih = 36, h = st->count * ih + 8;
    REAL y = a->y + a->h + 4;
    if (y + h > WIN_H - 8) y = a->y - h - 4;
    rrect(gr, a->x + 2, y + 4, a->w, h, 8, 0x80000000, 0, 0);
    rrect(gr, a->x, y, a->w, h, 8, 0xFF1A1714, kGold, 1);
    for (int i = 0; i < st->count; i++) {
        const REAL iy = y + 4 + i * ih;
        const int id = RID(R_DROPITEM, i), hv = hovered(id), sel = st->value == i;
        if (hv) rrect(gr, a->x + 4, iy, a->w - 8, ih, 5, 0xFF2A241C, 0, 0);
        text(gr, st->values[i], g.f_value, sel ? kGold : kText, g.fmt_left, a->x + 14, iy, a->w - 28, ih);
        reg(a->x, iy, a->w, ih, id);
    }
}

static void place_edits(void)
{
    const int show = g.page == P_ADV;
    HWND e[3] = {g.ed_log, g.ed_env, g.ed_cmd};
    RECT* r[3] = {&s_box_log, &s_box_env, &s_box_cmd};
    for (int i = 0; i < 3; i++) {
        if (!e[i]) continue;
        if (show) SetWindowPos(e[i], NULL, r[i]->left, r[i]->top, r[i]->right - r[i]->left, r[i]->bottom - r[i]->top,
                               SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
        else ShowWindow(e[i], SW_HIDE);
    }
}

static void paint_to(HDC hdc, const wchar_t* png)
{
    HDC mem = CreateCompatibleDC(hdc);
    HBITMAP bmp = CreateCompatibleBitmap(hdc, g.w, g.h);
    HGDIOBJ old = SelectObject(mem, bmp);
    GpGraphics* gr; GdipCreateFromHDC(mem, &gr); quality(gr);
    fill(gr, kBg, 0, 0, (REAL)g.w, (REAL)g.h);
    g.nreg = 0;

    /* page content, scrolled */
    const REAL sc = g.page == P_ADV || g.page == P_HOME ? 0 : g.scroll[g.page];
    GdipSetClipRect(gr, S(SIDE_W), 0, S(WIN_W - SIDE_W), S(WIN_H), CombineReplace);
    s_clip_y0 = 0; s_clip_y1 = WIN_H;
    REAL end = 0;
    switch (g.page) {
    case P_HOME: end = page_home(gr, 0); break;
    case P_GFX:  end = page_gfx(gr, 8 - sc) + sc; break;
    case P_PERF: end = page_perf(gr, 8 - sc) + sc; break;
    case P_GAME: end = page_game(gr, 8 - sc) + sc; break;
    case P_CTRL: end = page_ctrl(gr, 8 - sc) + sc; break;
    case P_ADV:  end = page_adv(gr, 8); break;
    }
    g.content_h[g.page] = end;
    /* scrollbar */
    if (end > WIN_H && g.page != P_ADV && g.page != P_HOME) {
        const REAL track = WIN_H - 24, th = track * WIN_H / end, ty = 12 + (track - th) * (sc / (end - WIN_H));
        rrect(gr, WIN_W - 9, ty, 4, th, 2, 0x50FFFFFF, 0, 0);
    }
    GdipResetClip(gr);
    sidebar(gr);
    chrome(gr);
    dropdown(gr);
    line(gr, 0x40C9AA71, 1, 0, 0, (REAL)g.w, 0);

    GdipDeleteGraphics(gr);
    if (png) {
        static const CLSID kPng = {0x557CF406, 0x1A04, 0x11D3, {0x9A, 0x73, 0x00, 0x00, 0xF8, 0x1E, 0xF3, 0x2E}};
        SelectObject(mem, old);
        GpImage* shot = NULL;
        if (GdipCreateBitmapFromHBITMAP(bmp, NULL, &shot) == 0) { GdipSaveImageToFile(shot, png, &kPng, NULL); GdipDisposeImage(shot); }
        DeleteObject(bmp); DeleteDC(mem);
        return;
    }
    BitBlt(hdc, 0, 0, g.w, g.h, mem, 0, 0, SRCCOPY);
    SelectObject(mem, old); DeleteObject(bmp); DeleteDC(mem);
    place_edits();
}

/* ---- actions --------------------------------------------------------------- */
static void redraw(void) { InvalidateRect(g.hwnd, NULL, FALSE); }
static void changed(void) { save_settings(); refresh_cmd_box(); redraw(); }
static void set_value(int si, int v)
{
    if (v < 0 || v >= s_set[si].count || v == s_set[si].value) return;
    s_set[si].value = v;
    if (si == S_LOGFILTER) refresh_log(1);
    changed();
}
static void go_page(int p)
{
    g.drop = -1; g.cap_row = -1;
    if (p == P_GAME || p == P_HOME) scan_saves();
    if (p == P_ADV) { refresh_log(1); refresh_cmd_box(); }
    g.page = p;
    redraw();
}
static int confirm(const wchar_t* msg) { return MessageBoxW(g.hwnd, msg, L"Demon's Souls", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) == IDYES; }
static void open_path(const wchar_t* p) { ShellExecuteW(g.hwnd, L"open", p, NULL, NULL, SW_SHOWNORMAL); }

static int pick_rpcs3(void)
{
    BROWSEINFOW bi = {0};
    bi.hwndOwner = g.hwnd;
    bi.lpszTitle = L"Pasta do RPCS3 (com o firmware do PS3 instalado)";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    PIDLIST_ABSOLUTE id = SHBrowseForFolderW(&bi);
    if (!id) return 0;
    wchar_t dir[MAX_PATH];
    const BOOL ok = SHGetPathFromIDListW(id, dir);
    CoTaskMemFree((void*)id);
    if (!ok) return 0;
    wchar_t probe[MAX_PATH]; path_join(probe, dir, L"dev_flash");
    if (!file_exists(probe) && !confirm(L"Essa pasta não tem dev_flash (o firmware do PS3 instalado no RPCS3). Sem ele o jogo fica sem música e vozes.\n\nUsar mesmo assim?"))
        return 0;
    wcscpy(g.rpcs3, dir);
    save_rpcs3();
    return 1;
}

static void play(void)
{
    const wchar_t* why;
    if (!game_ready(&why)) {
        MessageBoxW(g.hwnd, wcscmp(why, L"Falta compilar") == 0 ? L"O jogo ainda não foi compilado (scripts\\windows\\build.ps1)."
                                                               : L"O código do jogo ainda não foi gerado (scripts\\windows\\prepare.ps1).",
                    L"Demon's Souls", MB_ICONWARNING);
        return;
    }
    if (g.game) return;
    if (!g.rpcs3[0] && !pick_rpcs3()) return;
    save_settings(); keymap_save();
    if (s_set[S_AUTOBACKUP].value) { scan_saves(); if (g.nslots) backup_slots(-1, L"auto-"); prune_auto_backups(); }
    static wchar_t cmd[4096];
    build_command(cmd, 4096, 0);
    env_pair e[48]; const int n = build_env(e, 48);
    for (int i = 0; i < n; i++) SetEnvironmentVariableW(e[i].name, e[i].value[0] ? e[i].value : NULL);
    STARTUPINFOW si = {sizeof si};
    PROCESS_INFORMATION pi;
    if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, g.root, &si, &pi)) {
        wchar_t msg[256];
        _snwprintf(msg, 256, L"Não foi possível iniciar o jogo (erro %lu).", GetLastError());
        MessageBoxW(g.hwnd, msg, L"Demon's Souls", MB_ICONERROR);
        return;
    }
    CloseHandle(pi.hThread);
    g.game = pi.hProcess;
    SetTimer(g.hwnd, 4, 500, NULL);
    ShowWindow(g.hwnd, SW_MINIMIZE);
    redraw();
}
static void stop_game(void)
{
    if (!confirm(L"Fechar o jogo agora? O progresso que não foi salvo pelo jogo se perde.")) return;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W pe = {sizeof pe};
    if (snap != INVALID_HANDLE_VALUE && Process32FirstW(snap, &pe)) {
        do if (!_wcsicmp(pe.szExeFile, L"MyGameRecomp.exe")) {
            HANDLE p = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
            if (p) { TerminateProcess(p, 1); CloseHandle(p); }
        } while (Process32NextW(snap, &pe));
    }
    if (snap != INVALID_HANDLE_VALUE) CloseHandle(snap);
}

static void copy_log(void)
{
    const int n = GetWindowTextLengthW(g.ed_log);
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, sizeof(wchar_t) * (size_t)(n + 1));
    if (!h) return;
    GetWindowTextW(g.ed_log, (wchar_t*)GlobalLock(h), n + 1); GlobalUnlock(h);
    if (OpenClipboard(g.hwnd)) { EmptyClipboard(); SetClipboardData(CF_UNICODETEXT, h); CloseClipboard(); }
    else GlobalFree(h);
}

static void action(int a)
{
    wchar_t p[MAX_PATH], q[MAX_PATH], msg[512];
    if (a >= A_BK_DELETE) {
        const int i = a - A_BK_DELETE; if (i >= g.nbackups) return;
        _snwprintf(msg, 512, L"Mandar o backup %s para a Lixeira?", g.backups[i].name);
        if (!confirm(msg)) return;
        backups_dir(p); path_join(q, p, g.backups[i].name); shell_op(FO_DELETE, q, NULL, 1);
        scan_saves(); redraw(); return;
    }
    if (a >= A_BK_RESTORE) {
        const int i = a - A_BK_RESTORE; if (i >= g.nbackups) return;
        if (g.game) { MessageBoxW(g.hwnd, L"Feche o jogo antes de restaurar um backup.", L"Demon's Souls", MB_ICONWARNING); return; }
        _snwprintf(msg, 512, L"Restaurar o backup %s?\n\nOs saves com o mesmo nome são substituídos. Antes, os saves atuais vão para um backup novo (antes-de-restaurar).", g.backups[i].name);
        if (!confirm(msg)) return;
        wchar_t name[64]; wcscpy(name, g.backups[i].name);
        backup_slots(-1, L"antes-de-restaurar-");
        wchar_t sd[MAX_PATH], src[MAX_PATH]; savedata_dir(sd);
        SHCreateDirectoryExW(NULL, sd, NULL);
        backups_dir(p); path_join(q, p, name); path_join(src, q, L"*");
        if (!shell_op(FO_COPY, src, sd, 0)) MessageBoxW(g.hwnd, L"A cópia não terminou: confira a pasta de saves.", L"Demon's Souls", MB_ICONERROR);
        scan_saves(); redraw(); return;
    }
    if (a >= A_SLOT_DELETE) {
        const int i = a - A_SLOT_DELETE; if (i >= g.nslots) return;
        if (g.game) { MessageBoxW(g.hwnd, L"Feche o jogo antes de apagar um save.", L"Demon's Souls", MB_ICONWARNING); return; }
        _snwprintf(msg, 512, L"Apagar o save %s (%s)?\n\nAntes, ele é copiado para um backup; a pasta vai para a Lixeira.", g.slots[i].dir, g.slots[i].sub[0] ? g.slots[i].sub : g.slots[i].title);
        if (!confirm(msg)) return;
        backup_slots(i, L"antes-de-apagar-");
        savedata_dir(p); path_join(q, p, g.slots[i].dir); shell_op(FO_DELETE, q, NULL, 1);
        scan_saves(); redraw(); return;
    }
    if (a >= A_SLOT_BACKUP) {
        const int i = a - A_SLOT_BACKUP; if (i >= g.nslots) return;
        if (!backup_slots(i, L"")) MessageBoxW(g.hwnd, L"O backup falhou.", L"Demon's Souls", MB_ICONERROR);
        scan_saves(); redraw(); return;
    }
    switch (a) {
    case A_PLAY: play(); break;
    case A_STOP: stop_game(); break;
    case A_MIN: ShowWindow(g.hwnd, SW_MINIMIZE); break;
    case A_CLOSE: DestroyWindow(g.hwnd); break;
    case A_ALLGFX: go_page(P_GFX); break;
    case A_MANAGE_SAVES: go_page(P_GAME); break;
    case A_OPEN_SAVES: savedata_dir(p); SHCreateDirectoryExW(NULL, p, NULL); open_path(p); break;
    case A_OPEN_BACKUPS: backups_dir(p); SHCreateDirectoryExW(NULL, p, NULL); open_path(p); break;
    case A_VIEW_LOG: go_page(P_ADV); break;
    case A_OPEN_LOG: log_path(p); if (file_exists(p)) ShellExecuteW(g.hwnd, L"open", L"notepad.exe", p, NULL, SW_SHOWNORMAL); break;
    case A_LOG_COPY: copy_log(); break;
    case A_BACKUP_ALL:
        scan_saves();
        if (!g.nslots) MessageBoxW(g.hwnd, L"Não há saves para copiar.", L"Demon's Souls", MB_ICONINFORMATION);
        else if (!backup_slots(-1, L"")) MessageBoxW(g.hwnd, L"O backup falhou.", L"Demon's Souls", MB_ICONERROR);
        scan_saves(); redraw(); break;
    case A_RPCS3: if (pick_rpcs3()) redraw(); break;
    case A_CTRL_DEFAULTS:
        if (confirm(L"Voltar todos os controles para o padrão?")) { keymap_defaults(); keymap_save(); redraw(); }
        break;
    case A_CTRL_PC:
        if (confirm(L"Usar o layout de PC no teclado?\n\nWASD move, setas giram a câmera, Enter/E confirmam (✕), Espaço/Backspace voltam (○), "
                    L"mouse esquerdo/direito atacam (R1/R2), Shift/Ctrl esquerdos são L1/L2, R e F são □ e △, "
                    L"botão do meio ou Q travam a mira (R3), Tab é START, 1-4 o direcional.\n\nO controle não muda.")) {
            memcpy(s_km_vk, k_km_vk_pc, sizeof s_km_vk); keymap_save(); redraw();
        }
        break;
    case A_ALL_DEFAULTS:
        if (confirm(L"Voltar todas as opções do launcher para o padrão? (Controles, saves e variáveis extras ficam como estão.)")) {
            for (int i = 0; i < S_COUNT; i++) s_set[i].value = s_set[i].def;
            changed();
        }
        break;
    }
}

/* Bind: keyboard column takes a VK (moving it off any other row), controller
 * column an XInput source. */
static void bind(int row, int col, int value)
{
    if (col == 0) {
        for (int i = 0; i < KM_KEYS && value; i++) {          /* the key leaves any other control */
            if (s_km_vk[i][1] == value) s_km_vk[i][1] = 0;
            if (s_km_vk[i][0] == value) { s_km_vk[i][0] = s_km_vk[i][1]; s_km_vk[i][1] = 0; }
        }
        s_km_vk[row][0] = value; s_km_vk[row][1] = 0;
    } else {
        for (int i = 0; i < KM_BUTTONS; i++) if (value >= 0 && s_km_xs[i] == value) s_km_xs[i] = XS_NONE;
        s_km_xs[row] = value;
    }
    g.cap_row = -1;
    keymap_save();
    redraw();
}
static void start_capture(int row, int col)
{
    g.cap_row = row; g.cap_col = col; g.cap_until = GetTickCount64() + 6000;
    XINPUT_STATE st; memset(&st, 0, sizeof st);
    if (pad_state(&st)) { g.pad_prev = st.Gamepad.wButtons; g.pad_prev_lt = st.Gamepad.bLeftTrigger > 30; g.pad_prev_rt = st.Gamepad.bRightTrigger > 30; }
    else { g.pad_prev = 0xFFFF; g.pad_prev_lt = g.pad_prev_rt = 1; }
    redraw();
}

static void scroll_by(REAL d)
{
    if (g.page == P_ADV || g.page == P_HOME) return;
    const REAL max = g.content_h[g.page] - WIN_H;
    REAL s = g.scroll[g.page] + d;
    if (s > max) s = max;
    if (s < 0) s = 0;
    if (s != g.scroll[g.page]) { g.scroll[g.page] = s; g.drop = -1; redraw(); }
}

static void slider_to(int si, int px)
{
    const region* r = find_region(RID(R_SLIDER, si));
    if (!r) return;
    const REAL x0 = r->x + 8, w = r->w - 16;
    REAL t = (px / g.s - x0) / w;
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    int v = (int)(t * (s_set[si].count - 1) + 0.5f);
    v = (v + 2) / 5 * 5;                         /* 5% steps */
    set_value(si, v);
}

static void click(int px, int py)
{
    const int id = hit_test(px, py);
    if (g.drop >= 0) {
        const int was = g.drop; g.drop = -1;
        if (RKIND(id) == R_DROPITEM) { set_value(was, RIDX(id)); return; }
        redraw();
        if (RKIND(id) == R_LIST && RIDX(id) == was) return;
    }
    if (g.cap_row >= 0) {
        /* a left click on the cell being captured binds the left button */
        if (RKIND(id) == R_BIND && RIDX(id) == g.cap_row * 2 && g.cap_col == 0) { bind(g.cap_row, 0, VK_LBUTTON); return; }
        g.cap_row = -1; redraw();
        if (RKIND(id) == R_BIND) return;
    }
    switch (RKIND(id)) {
    case R_NAV: go_page(RIDX(id)); break;
    case R_LIST: g.drop = RIDX(id); redraw(); break;
    case R_TOGGLE: set_value(RIDX(id), !s_set[RIDX(id)].value); break;
    case R_SLIDER: g.drag = RIDX(id); SetCapture(g.hwnd); slider_to(g.drag, px); break;
    case R_ACTION: action(RIDX(id)); break;
    case R_BIND: start_capture(RIDX(id) / 2, RIDX(id) % 2); break;
    }
}

/* ---- timers: pad capture, live highlight, game state ------------------- */
static void poll(void)
{
    const int fg = GetForegroundWindow() == g.hwnd;
    XINPUT_STATE st; memset(&st, 0, sizeof st);
    const int pad = fg && pad_state(&st);
    if (g.cap_row >= 0) {
        if (GetTickCount64() > g.cap_until) { g.cap_row = -1; redraw(); return; }
        if (g.cap_col == 1 && pad) {
            const WORD now = st.Gamepad.wButtons, newly = now & ~g.pad_prev;
            const int lt = st.Gamepad.bLeftTrigger > 30, rt = st.Gamepad.bRightTrigger > 30;
            int src = -1;
            for (int x = 0; x < XS_COUNT && src < 0; x++) if (k_xs_bit[x] && (newly & k_xs_bit[x])) src = x;
            if (src < 0 && lt && !g.pad_prev_lt) src = 6;
            if (src < 0 && rt && !g.pad_prev_rt) src = 7;
            g.pad_prev = now; g.pad_prev_lt = lt; g.pad_prev_rt = rt;
            if (src >= 0) { bind(g.cap_row, 1, src); return; }
        }
    }
    if (g.page == P_CTRL && fg) {
        unsigned live = 0;
        for (int r = 0; r < KM_KEYS; r++)
            if (km_held(r) || (pad && r < KM_BUTTONS && xs_down(&st, s_km_xs[r])))
                live |= 1u << r;
        if (live != g.live_keys) { g.live_keys = live; redraw(); }
    } else if (pad && g.page == P_HOME && !g.game && g.cap_row < 0) {
        const WORD now = st.Gamepad.wButtons;
        if ((now & XINPUT_GAMEPAD_START) && !(g.pad_prev & XINPUT_GAMEPAD_START)) play();
        g.pad_prev = now;
    }
    if (g.saved_flash && ++g.saved_flash > 40) { g.saved_flash = 0; redraw(); }
}

static LRESULT CALLBACK edit_proc(HWND h, UINT m, WPARAM wp, LPARAM lp, UINT_PTR id, DWORD_PTR data)
{
    (void)id; (void)data;
    if (m == WM_KEYDOWN && wp == 'A' && GetKeyState(VK_CONTROL) < 0) { SendMessageW(h, EM_SETSEL, 0, -1); return 0; }
    return DefSubclassProc(h, m, wp, lp);
}

/* ---- window ---------------------------------------------------------------- */
static LRESULT CALLBACK wndproc(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    switch (m) {
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: { PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps); paint_to(dc, NULL); EndPaint(h, &ps); return 0; }
    case WM_NCHITTEST: {
        POINT p = {(SHORT)LOWORD(lp), (SHORT)HIWORD(lp)}; ScreenToClient(h, &p);
        const REAL y = p.y / g.s, x = p.x / g.s;
        if (!hit_test(p.x, p.y) && (y < 50 || (x < SIDE_W && y < 96))) return HTCAPTION;
        return HTCLIENT;
    }
    case WM_MOUSEMOVE: {
        const int px = (SHORT)LOWORD(lp), py = (SHORT)HIWORD(lp);
        if (g.drag >= 0) { slider_to(g.drag, px); return 0; }
        const int id = hit_test(px, py);
        if (id != g.hover) { g.hover = id; redraw(); }
        TRACKMOUSEEVENT t = {sizeof t, TME_LEAVE, h, 0}; TrackMouseEvent(&t);
        return 0;
    }
    case WM_MOUSELEAVE: if (g.hover) { g.hover = 0; redraw(); } return 0;
    case WM_LBUTTONDOWN: SetFocus(h); click((SHORT)LOWORD(lp), (SHORT)HIWORD(lp)); return 0;
    case WM_LBUTTONUP: if (g.drag >= 0) { g.drag = -1; ReleaseCapture(); redraw(); } return 0;
    case WM_RBUTTONDOWN: case WM_MBUTTONDOWN: case WM_XBUTTONDOWN:
        if (g.cap_row >= 0 && g.cap_col == 0)
            bind(g.cap_row, 0, m == WM_RBUTTONDOWN ? VK_RBUTTON : m == WM_MBUTTONDOWN ? VK_MBUTTON
                              : GET_XBUTTON_WPARAM(wp) == XBUTTON1 ? VK_XBUTTON1 : VK_XBUTTON2);
        return m == WM_XBUTTONDOWN ? TRUE : 0;
    case WM_MOUSEWHEEL: {
        if (g.drop >= 0) { g.drop = -1; redraw(); }
        scroll_by(-GET_WHEEL_DELTA_WPARAM(wp) / 120.0f * 60);
        return 0;
    }
    case WM_KEYDOWN: case WM_SYSKEYDOWN:
        if (g.cap_row >= 0 && g.cap_col == 0) {
            if (wp == VK_ESCAPE) { g.cap_row = -1; redraw(); }
            else if (wp == VK_DELETE || wp == VK_BACK) bind(g.cap_row, 0, 0);
            else {
                int vk = (int)wp;
                if (vk == VK_SHIFT) vk = (MapVirtualKeyW((lp >> 16) & 0xFF, MAPVK_VSC_TO_VK_EX) == VK_RSHIFT) ? VK_RSHIFT : VK_LSHIFT;
                else if (vk == VK_CONTROL) vk = (lp & (1 << 24)) ? VK_RCONTROL : VK_LCONTROL;
                bind(g.cap_row, 0, vk);
            }
            return 0;
        }
        if (g.cap_row >= 0 && (wp == VK_ESCAPE || wp == VK_DELETE || wp == VK_BACK)) {
            if (wp == VK_ESCAPE) { g.cap_row = -1; redraw(); } else bind(g.cap_row, 1, XS_NONE);
            return 0;
        }
        if (wp == VK_ESCAPE) { if (g.drop >= 0) { g.drop = -1; redraw(); } return 0; }
        if (wp == VK_RETURN && g.page == P_HOME) { if (!g.game) play(); return 0; }
        if (wp == VK_NEXT) scroll_by(400); else if (wp == VK_PRIOR) scroll_by(-400);
        else if (wp == VK_DOWN) scroll_by(60); else if (wp == VK_UP) scroll_by(-60);
        else if (wp == VK_TAB) go_page((g.page + (GetKeyState(VK_SHIFT) < 0 ? P_COUNT - 1 : 1)) % P_COUNT);
        return 0;
    case WM_COMMAND:
        if ((HWND)lp == g.ed_env && HIWORD(wp) == EN_CHANGE) {
            const int n = GetWindowTextLengthW(g.ed_env);
            wchar_t* t = (wchar_t*)calloc((size_t)n + 1, sizeof(wchar_t));
            GetWindowTextW(g.ed_env, t, n + 1);
            FILE* f = _wfopen(g.envfile, L"w, ccs=UTF-8");
            if (f) { for (wchar_t* c = t; *c; c++) if (*c != L'\r') fputwc(*c, f); fclose(f); }
            free(t);
            g.saved_flash = 1;
            refresh_cmd_box();
        }
        return 0;
    case WM_CTLCOLOREDIT: case WM_CTLCOLORSTATIC:
        SetTextColor((HDC)wp, (HWND)lp == g.ed_env ? RGB(232, 226, 212) : RGB(190, 182, 168));
        SetBkColor((HDC)wp, RGB(15, 14, 12));
        return (LRESULT)g.edit_bg;
    case WM_TIMER:
        if (wp == 1) poll();
        else if (wp == 2) {
            g.alpha = (BYTE)min(255, g.alpha + 32);
            SetLayeredWindowAttributes(h, 0, g.alpha, LWA_ALPHA);
            if (g.alpha == 255) KillTimer(h, 2);
        } else if (wp == 3) { if (g.page == P_ADV) refresh_log(0); }
        else if (wp == 4 && g.game && WaitForSingleObject(g.game, 0) == WAIT_OBJECT_0) {
            CloseHandle(g.game); g.game = NULL; KillTimer(h, 4);
            scan_saves(); refresh_log(1);
            ShowWindow(h, SW_RESTORE); SetForegroundWindow(h);
            redraw();
        }
        return 0;
    case WM_DESTROY: save_settings(); PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h, m, wp, lp);
}

static GpFont* font(GpFontFamily* fam, REAL px, int style)
{ GpFont* f = NULL; GdipCreateFont(fam, S(px), style, UnitPixel, &f); return f; }
static GpFontFamily* family(GpFontCollection* fc, const wchar_t* name, const wchar_t* fallback)
{
    GpFontFamily* f = NULL;
    if (fc && GdipCreateFontFamilyFromName(name, fc, &f) == 0) return f;
    if (fallback && GdipCreateFontFamilyFromName(fallback, NULL, &f) == 0) return f;
    GdipCreateFontFamilyFromName(L"Segoe UI", NULL, &f);
    return f;
}
static HWND make_edit(HWND parent, DWORD style)
{
    HWND e = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_NOHIDESEL | style,
                             0, 0, 10, 10, parent, NULL, GetModuleHandleW(NULL), NULL);
    SendMessageW(e, WM_SETFONT, (WPARAM)g.mono, TRUE);
    SendMessageW(e, EM_SETLIMITTEXT, 0x7FFFFFFE, 0);
    SetWindowSubclass(e, edit_proc, 1, 0);
    SetWindowTheme(e, L"DarkMode_Explorer", NULL);   /* dark scroll bars (Windows 10 1809+) */
    return e;
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE prev, PWSTR args, int show)
{
    (void)prev; (void)args; (void)show;
    BOOL (WINAPI *dpi_ctx)(HANDLE) = (BOOL (WINAPI*)(HANDLE))GetProcAddress(GetModuleHandleW(L"user32"), "SetProcessDpiAwarenessContext");
    if (dpi_ctx) dpi_ctx((HANDLE)-4 /* PER_MONITOR_AWARE_V2 */); else SetProcessDPIAware();
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    find_root();
    load_settings();
    keymap_load();
    g.drop = g.drag = g.cap_row = -1;

    ULONG_PTR token; GdiplusStartupInput gi = {1, NULL, FALSE, FALSE};
    if (GdiplusStartup(&token, &gi, NULL) != 0) return 1;

    /* Size: 1280x800 at the monitor's DPI, kept inside 92% of the work area. */
    POINT origin = {0, 0};
    HMONITOR mon = MonitorFromPoint(origin, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi = {sizeof mi}; GetMonitorInfoW(mon, &mi);
    const int ww = mi.rcWork.right - mi.rcWork.left, wh = mi.rcWork.bottom - mi.rcWork.top;
    HDC sdc = GetDC(NULL); g.s = GetDeviceCaps(sdc, LOGPIXELSX) / 96.0f;
    g.s = min(g.s, min(ww * 0.92f / WIN_W, wh * 0.92f / WIN_H));
    g.w = (int)(WIN_W * g.s); g.h = (int)(WIN_H * g.s);
    GdipCreateFromHDC(sdc, &g.measure);

    wchar_t fonts[MAX_PATH], file[MAX_PATH];
    path_join(fonts, g.root, L"port\\ui\\res\\fonts");
    GpFontCollection* fc = NULL;
    if (GdipNewPrivateFontCollection(&fc) == 0) {
        static const wchar_t* const files[] = {L"Cinzel.ttf", L"AlegreyaSans-Regular.ttf", L"AlegreyaSans-Bold.ttf"};
        for (int i = 0; i < 3; i++) { path_join(file, fonts, files[i]); if (file_exists(file)) GdipPrivateAddFontFile(fc, file); }
    }
    g.f_head_fam = family(fc, L"Cinzel", L"Georgia");
    g.f_text_fam = family(fc, L"Alegreya Sans", L"Segoe UI");
    g.f_icon_fam = family(NULL, L"Segoe Fluent Icons", L"Segoe MDL2 Assets");
    g.f_sym_fam = family(NULL, L"Segoe UI Symbol", L"Segoe UI");
    g.f_logo = font(g.f_head_fam, 21, FontBold);
    g.f_hero = font(g.f_head_fam, 64, FontRegular);
    g.f_page = font(g.f_head_fam, 36, FontRegular);
    g.f_nav = font(g.f_head_fam, 15, FontBold);
    g.f_section = font(g.f_head_fam, 15, FontBold);
    g.f_button = font(g.f_head_fam, 22, FontBold);
    g.f_label = font(g.f_text_fam, 19, FontBold);
    g.f_value = font(g.f_text_fam, 17, FontRegular);
    g.f_help = font(g.f_text_fam, 15.5f, FontRegular);
    g.f_small = font(g.f_text_fam, 15, FontRegular);
    g.f_tiny = font(g.f_text_fam, 13, FontRegular);
    g.f_icon = font(g.f_icon_fam, 16, FontRegular);
    g.f_sym = font(g.f_sym_fam, 16, FontRegular);
    GdipCreateStringFormat(StringNoWrap, 0, &g.fmt_left);
    GdipSetStringFormatLineAlign(g.fmt_left, AlignCenter);
    GdipCreateStringFormat(StringNoWrap, 0, &g.fmt_center);
    GdipSetStringFormatAlign(g.fmt_center, AlignCenter); GdipSetStringFormatLineAlign(g.fmt_center, AlignCenter);
    GdipCreateStringFormat(StringNoWrap, 0, &g.fmt_right);
    GdipSetStringFormatAlign(g.fmt_right, AlignFar); GdipSetStringFormatLineAlign(g.fmt_right, AlignCenter);
    GdipCreateStringFormat(StringNoWrap, 0, &g.fmt_ell);
    GdipSetStringFormatLineAlign(g.fmt_ell, AlignCenter); GdipSetStringFormatTrimming(g.fmt_ell, TrimEllipsis);
    GdipCreateStringFormat(0, 0, &g.fmt_wrap);

    wchar_t pic[MAX_PATH];
    path_join(pic, g.work, L"disc\\PS3_GAME\\PIC1.PNG");
    g.hero = file_exists(pic) ? load_image_copy(pic) : NULL;
    read_info();
    scan_saves();

    g.xinput = LoadLibraryW(L"xinput1_4.dll");
    if (!g.xinput) g.xinput = LoadLibraryW(L"xinput9_1_0.dll");
    if (g.xinput) g.xget = (DWORD (WINAPI*)(DWORD, XINPUT_STATE*))GetProcAddress(g.xinput, "XInputGetState");

    wchar_t shot[MAX_PATH];
    if (GetEnvironmentVariableW(L"DES_LAUNCHER_SHOT", shot, MAX_PATH)) {
        wchar_t f[16];
        if (GetEnvironmentVariableW(L"DES_LAUNCHER_PAGE", f, 16)) g.page = _wtoi(f) % P_COUNT;
        if (GetEnvironmentVariableW(L"DES_LAUNCHER_DROP", f, 16)) g.drop = _wtoi(f);
        if (GetEnvironmentVariableW(L"DES_LAUNCHER_SCROLL", f, 16)) g.scroll[g.page] = (REAL)_wtoi(f);
        paint_to(sdc, shot);                         /* first pass lays out regions (the dropdown anchors on them) */
        paint_to(sdc, shot);
        ReleaseDC(NULL, sdc);
        GdiplusShutdown(token);
        return 0;
    }
    ReleaseDC(NULL, sdc);

    WNDCLASSW wc = {0};
    wc.lpfnWndProc = wndproc; wc.hInstance = inst; wc.lpszClassName = L"DesLauncher";
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
    RegisterClassW(&wc);
    g.hwnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_APPWINDOW, wc.lpszClassName, L"Demon's Souls",
        WS_POPUP | WS_MINIMIZEBOX | WS_SYSMENU | WS_CLIPCHILDREN,
        mi.rcWork.left + (ww - g.w) / 2, mi.rcWork.top + (wh - g.h) / 2, g.w, g.h, NULL, NULL, inst, NULL);
    if (!g.hwnd) return 1;

    g.mono = CreateFontW(-(int)(13 * g.s), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, FIXED_PITCH, L"Consolas");
    g.edit_bg = CreateSolidBrush(RGB(15, 14, 12));
    g.ed_log = make_edit(g.hwnd, ES_READONLY | WS_HSCROLL | ES_AUTOHSCROLL);
    g.ed_env = make_edit(g.hwnd, ES_WANTRETURN);
    g.ed_cmd = make_edit(g.hwnd, ES_READONLY);
    {   /* the extra variables, as saved */
        FILE* f = _wfopen(g.envfile, L"r, ccs=UTF-8");
        if (f) {
            static wchar_t buf[16384]; int k = 0; wint_t c;
            while ((c = fgetwc(f)) != WEOF && k < 16380) { if (c == L'\n') buf[k++] = L'\r'; buf[k++] = (wchar_t)c; }
            buf[k] = 0; fclose(f);
            SetWindowTextW(g.ed_env, buf);
        }
    }
    refresh_cmd_box();
    g.saved_flash = 0;

    SetLayeredWindowAttributes(g.hwnd, 0, 0, LWA_ALPHA);
    ShowWindow(g.hwnd, SW_SHOW);
    SetTimer(g.hwnd, 1, 33, NULL);
    SetTimer(g.hwnd, 2, 15, NULL);
    SetTimer(g.hwnd, 3, 2000, NULL);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    GdiplusShutdown(token);
    CoUninitialize();
    return 0;
}
