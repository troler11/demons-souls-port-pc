/* Exercise the actual presentation worker with a real D3D12 swap chain.
 * The FIFO/window owner drains while DXGI can send it messages. Verify copied
 * real-frame pixels, one-pair backpressure, mode 2, and worker restart. */
#define COBJMACROS
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <initguid.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_4.h>
#include <stdio.h>
#include <assert.h>
#include <stdint.h>
typedef uint32_t u32;
typedef uint64_t u64;
typedef uint8_t u8;
static struct {
    ID3D12Device* dev; ID3D12CommandQueue* queue; IDXGISwapChain3* swap;
    ID3D12Resource* backbuf[2]; int ready;
} g;
static struct {
    ID3D12Resource* hist[2]; int cur, pending; LARGE_INTEGER qf, due;
    double interval, real_gap_ticks;
    unsigned long long generated_submitted, real_submitted, gap_count;
} s_fg;
static u32 g_ld_frames;
#define FG_SR (D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
static int mode = 1;
static int ld_fg_mode(void) { return mode; }
static UINT ld_present_interval(void) { return 0; }
static HANDLE pace_gate;
static void ld_present_pace(void) { if (pace_gate) WaitForSingleObject(pace_gate, INFINITE); Sleep(1); }
static volatile LONG measured;
static void ld_present_measure_record(u32 frame) { (void)frame; InterlockedIncrement(&measured); }
static void ld_dump_dred(const char* where, HRESULT hr) { fprintf(stderr, "%s: %lx\n", where, (unsigned long)hr); }
void ps3_name_host_thread(const char* name) { (void)name; }
#include "../third_party/ps3recomp/libs/video/rsx_framegen_present.h"
#define OK(x) do { HRESULT result = (x); if (FAILED(result)) { fprintf(stderr, "%s: %lx\n", #x, (unsigned long)result); abort(); } } while (0)
static void barrier(ID3D12GraphicsCommandList* l, ID3D12Resource* r, D3D12_RESOURCE_STATES a, D3D12_RESOURCE_STATES b)
{
    D3D12_RESOURCE_BARRIER v = {0}; v.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    v.Transition.pResource = r; v.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    v.Transition.StateBefore = a; v.Transition.StateAfter = b;
    l->lpVtbl->ResourceBarrier(l, 1, &v);
}
int main(void)
{
    ID3D12Debug* debug = NULL;
    if (SUCCEEDED(D3D12GetDebugInterface(&IID_ID3D12Debug, (void**)&debug))) { debug->lpVtbl->EnableDebugLayer(debug); debug->lpVtbl->Release(debug); }
    WNDCLASSA wc = {0}; wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(NULL); wc.lpszClassName = "FGWorkerRegression";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowA(wc.lpszClassName, "Framegen regression", WS_OVERLAPPEDWINDOW,
        0, 0, 160, 128, NULL, NULL, wc.hInstance, NULL);
    assert(hwnd); ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    IDXGIFactory4* factory; OK(CreateDXGIFactory1(&IID_IDXGIFactory4, (void**)&factory));
    OK(D3D12CreateDevice(NULL, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, (void**)&g.dev));
    D3D12_COMMAND_QUEUE_DESC qd = {0}; qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    OK(g.dev->lpVtbl->CreateCommandQueue(g.dev, &qd, &IID_ID3D12CommandQueue, (void**)&g.queue));
    DXGI_SWAP_CHAIN_DESC1 sd = {0}; sd.Width = sd.Height = 64; sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.SampleDesc.Count = 1; sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = 2; sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    IDXGISwapChain1* swap;
    OK(factory->lpVtbl->CreateSwapChainForHwnd(factory, (IUnknown*)g.queue, hwnd, &sd, NULL, NULL, &swap));
    OK(swap->lpVtbl->QueryInterface(swap, &IID_IDXGISwapChain3, (void**)&g.swap)); swap->lpVtbl->Release(swap);
    ID3D12DescriptorHeap* heap; D3D12_DESCRIPTOR_HEAP_DESC hd = {0}; hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; hd.NumDescriptors = 2;
    OK(g.dev->lpVtbl->CreateDescriptorHeap(g.dev, &hd, &IID_ID3D12DescriptorHeap, (void**)&heap));
    D3D12_CPU_DESCRIPTOR_HANDLE handle; heap->lpVtbl->GetCPUDescriptorHandleForHeapStart(heap, &handle);
    const UINT step = g.dev->lpVtbl->GetDescriptorHandleIncrementSize(g.dev, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    D3D12_RESOURCE_DESC image = {0}; image.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    image.Width = image.Height = 64; image.DepthOrArraySize = image.MipLevels = 1;
    image.Format = sd.Format; image.SampleDesc.Count = 1;
    D3D12_HEAP_PROPERTIES hp = {0}; hp.Type = D3D12_HEAP_TYPE_DEFAULT;
    for (UINT i = 0; i < 2; i++) {
        OK(g.swap->lpVtbl->GetBuffer(g.swap, i, &IID_ID3D12Resource, (void**)&g.backbuf[i]));
        D3D12_CPU_DESCRIPTOR_HANDLE rtv = {handle.ptr + i * step};
        g.dev->lpVtbl->CreateRenderTargetView(g.dev, g.backbuf[i], NULL, rtv);
        OK(g.dev->lpVtbl->CreateCommittedResource(g.dev, &hp, D3D12_HEAP_FLAG_NONE, &image,
            FG_SR, NULL, &IID_ID3D12Resource, (void**)&s_fg.hist[i]));
    }
    ID3D12CommandAllocator* alloc; ID3D12GraphicsCommandList* list; ID3D12Fence* fence;
    OK(g.dev->lpVtbl->CreateCommandAllocator(g.dev, D3D12_COMMAND_LIST_TYPE_DIRECT, &IID_ID3D12CommandAllocator, (void**)&alloc));
    OK(g.dev->lpVtbl->CreateCommandList(g.dev, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc, NULL, &IID_ID3D12GraphicsCommandList, (void**)&list));
    OK(list->lpVtbl->Close(list));
    OK(g.dev->lpVtbl->CreateFence(g.dev, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void**)&fence));
    HANDLE event = CreateEventA(NULL, FALSE, FALSE, NULL); u64 serial = 0;
    D3D12_RESOURCE_DESC buffer = {0}; buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    buffer.Width = 256 * 64; buffer.Height = buffer.DepthOrArraySize = buffer.MipLevels = buffer.SampleDesc.Count = 1;
    buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR; hp.Type = D3D12_HEAP_TYPE_READBACK;
    ID3D12Resource* readback;
    OK(g.dev->lpVtbl->CreateCommittedResource(g.dev, &hp, D3D12_HEAP_FLAG_NONE, &buffer,
        D3D12_RESOURCE_STATE_COPY_DEST, NULL, &IID_ID3D12Resource, (void**)&readback));
    /* Plain presentation must work before any FG clock/history initialization,
     * even when FG's own async mode is disabled. Hold the consumer in pacing
     * to prove publication does not wait; drain is the backbuffer-use barrier. */
    g.ready = 1;
    _putenv_s("LD_FRAMEGEN_ASYNC", "0");
    pace_gate = CreateEventA(NULL, TRUE, FALSE, NULL);
    assert(pace_gate && fg_worker_init_for(0));
    const u32 plain_bbi = g.swap->lpVtbl->GetCurrentBackBufferIndex(g.swap);
    fg_worker_submit_plain();
    assert(s_fg_present.busy && measured == 0);
    assert(WaitForSingleObject(s_fg_present.done, 10) == WAIT_TIMEOUT);
    SetEvent(pace_gate);
    fg_worker_drain();
    assert(g.ready && measured == 1 && !s_fg_present.busy);
    assert(g.swap->lpVtbl->GetCurrentBackBufferIndex(g.swap) != plain_bbi);
    assert(s_fg.generated_submitted == 0 && s_fg.real_submitted == 0);
    assert(!s_fg.qf.QuadPart);
    fg_worker_shutdown();
    CloseHandle(pace_gate); pace_gate = NULL;
    _putenv_s("LD_FRAMEGEN_ASYNC", "1");
    measured = 0;
    QueryPerformanceFrequency(&s_fg.qf); s_fg.interval = s_fg.qf.QuadPart * 0.008;
    g.ready = 1;
    for (unsigned n = 0; n < 64; n++) {
        if (n == 32) { fg_worker_shutdown(); mode = 2; }
        assert(fg_worker_init());
        fg_worker_drain(); assert(!s_fg_present.busy && g.ready);
        OK(alloc->lpVtbl->Reset(alloc)); OK(list->lpVtbl->Reset(list, alloc, NULL));
        const u32 bbi = g.swap->lpVtbl->GetCurrentBackBufferIndex(g.swap);
        const D3D12_CPU_DESCRIPTOR_HANDLE rtv = {handle.ptr + bbi * step};
        const float green[4] = {0, 1, 0, 1}, red[4] = {1, 0, 0, 1};
        barrier(list, g.backbuf[bbi], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
        list->lpVtbl->ClearRenderTargetView(list, rtv, green, 0, NULL);
        barrier(list, g.backbuf[bbi], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
        s_fg.cur = n & 1;
        barrier(list, s_fg.hist[s_fg.cur], FG_SR, D3D12_RESOURCE_STATE_COPY_DEST);
        list->lpVtbl->CopyResource(list, s_fg.hist[s_fg.cur], g.backbuf[bbi]);
        barrier(list, s_fg.hist[s_fg.cur], D3D12_RESOURCE_STATE_COPY_DEST, FG_SR);
        barrier(list, g.backbuf[bbi], D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
        list->lpVtbl->ClearRenderTargetView(list, rtv, red, 0, NULL);
        barrier(list, g.backbuf[bbi], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
        OK(list->lpVtbl->Close(list)); ID3D12CommandList* submitted[] = {(ID3D12CommandList*)list};
        g.queue->lpVtbl->ExecuteCommandLists(g.queue, 1, submitted);
        s_fg.pending = 1; g_ld_frames = n;
        fg_worker_submit(); assert(s_fg_present.busy && !s_fg.pending);
        fg_worker_drain(); assert(g.ready);
        assert(s_fg_present.job.generated_count == 1);
        assert(s_fg_present.job.real_count == (mode == 1));
        /* Validate that mode 1 really copied the green real frame. */
        OK(g.queue->lpVtbl->Signal(g.queue, fence, ++serial));
        OK(fence->lpVtbl->SetEventOnCompletion(fence, serial, event)); WaitForSingleObject(event, INFINITE);
        if (mode == 1) {
            OK(alloc->lpVtbl->Reset(alloc)); OK(list->lpVtbl->Reset(list, alloc, NULL));
            ID3D12Resource* real_bb = g.backbuf[bbi ^ 1u];
            barrier(list, real_bb, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_SOURCE);
            D3D12_TEXTURE_COPY_LOCATION src = {0}, dst = {0};
            src.pResource = real_bb; src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            dst.pResource = readback; dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            dst.PlacedFootprint.Footprint.Format = sd.Format;
            dst.PlacedFootprint.Footprint.Width = dst.PlacedFootprint.Footprint.Height = 64;
            dst.PlacedFootprint.Footprint.Depth = 1; dst.PlacedFootprint.Footprint.RowPitch = 256;
            list->lpVtbl->CopyTextureRegion(list, &dst, 0, 0, 0, &src, NULL);
            barrier(list, real_bb, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PRESENT);
            OK(list->lpVtbl->Close(list)); g.queue->lpVtbl->ExecuteCommandLists(g.queue, 1, submitted);
            OK(g.queue->lpVtbl->Signal(g.queue, fence, ++serial));
            OK(fence->lpVtbl->SetEventOnCompletion(fence, serial, event)); WaitForSingleObject(event, INFINITE);
            u8* pixels; OK(readback->lpVtbl->Map(readback, 0, NULL, (void**)&pixels));
            assert(pixels[0] == 0 && pixels[1] == 255 && pixels[2] == 0);
            readback->lpVtbl->Unmap(readback, 0, NULL);
        }
    }
    fg_worker_shutdown(); assert(measured == 64);
    assert(s_fg.generated_submitted == 64 && s_fg.real_submitted == 32);
    ID3D12InfoQueue* info = NULL;
    if (SUCCEEDED(g.dev->lpVtbl->QueryInterface(g.dev, &IID_ID3D12InfoQueue, (void**)&info))) {
        const UINT64 messages = info->lpVtbl->GetNumStoredMessagesAllowedByRetrievalFilter(info);
        for (UINT64 i = 0; i < messages; i++) {
            SIZE_T bytes = 0; info->lpVtbl->GetMessage(info, i, NULL, &bytes);
            D3D12_MESSAGE* message = (D3D12_MESSAGE*)malloc(bytes);
            if (message && SUCCEEDED(info->lpVtbl->GetMessage(info, i, message, &bytes))) {
                if (message->Severity <= D3D12_MESSAGE_SEVERITY_ERROR) {
                    fprintf(stderr, "D3D12 validation: %s\n", message->pDescription); abort();
                }
            }
            free(message);
        }
        info->lpVtbl->Release(info);
        puts("D3D12 validation: no errors/corruption");
    }
    puts("present worker: plain frame without FG initialization, nonblocking publish, drain, 64 FG pairs, real pixels, mode 2 and restart passed");
    DestroyWindow(hwnd);
    return 0;
}
