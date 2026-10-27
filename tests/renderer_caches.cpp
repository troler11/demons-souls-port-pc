// Exercise the actual cache headers with isolated files and real DXBC/PSOs.
#define CINTERFACE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <initguid.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <cassert>
#include <string>
#include <vector>
#include "rsx_shader_preload.h"
#include "rsx_pipeline_library.h"
#define RELEASE(p) do { if(p) { (p)->lpVtbl->Release(p); (p)=nullptr; } } while(0)
static const uint32_t magic=0x43535A59u;
static ID3DBlob* compile(const char* source,const char* target) {
    ID3DBlob *blob=nullptr,*error=nullptr;
    HRESULT hr=D3DCompile(source,strlen(source),nullptr,nullptr,nullptr,"main",target,0,0,&blob,&error);
    if(FAILED(hr))fprintf(stderr,"%s\n",error?(const char*)error->lpVtbl->GetBufferPointer(error):"compile failed");
    RELEASE(error);assert(SUCCEEDED(hr)&&blob);return blob;
}
static void fixture(const std::string& directory,const std::string& source,ID3DBlob* blob) {
    ld_shader_file_header h={};h.magic=magic;h.version=2;h.stage='P';h.source_length=(uint32_t)source.size();
    h.blob_length=(uint32_t)blob->lpVtbl->GetBufferSize(blob);h.source_hash=ld_cache_hash(source.data(),source.size(),1469598103934665603ull);
    char name[100];snprintf(name,sizeof name,"/P_%016llX_%08X.bin",(unsigned long long)h.source_hash,h.source_length);
    FILE* f=fopen((directory+name).c_str(),"wb");assert(f);
    assert(fwrite(&h,sizeof h,1,f)==1);assert(fwrite(source.data(),1,source.size(),f)==source.size());
    assert(fwrite(blob->lpVtbl->GetBufferPointer(blob),1,h.blob_length,f)==h.blob_length);assert(!fclose(f));
}
static void wait_preload() {assert(s_shpre.thread);assert(WaitForSingleObject(s_shpre.thread,15000)==WAIT_OBJECT_0);}
static void check_blob(const std::string& source,ID3DBlob* expected) {
    auto hash=ld_cache_hash(source.data(),source.size(),1469598103934665603ull);
    ID3DBlob* blob=ld_shpre_find('P',source.c_str(),(uint32_t)source.size(),hash);assert(blob);
    assert(blob->lpVtbl->GetBufferSize(blob)==expected->lpVtbl->GetBufferSize(expected));
    assert(!memcmp(blob->lpVtbl->GetBufferPointer(blob),expected->lpVtbl->GetBufferPointer(expected),blob->lpVtbl->GetBufferSize(blob)));
    RELEASE(blob);
    std::string different=source;different[0]^=1;
    blob=ld_shpre_find('P',different.c_str(),(uint32_t)different.size(),hash);assert(!blob); // hash alone never suffices
    blob=ld_shpre_find('V',source.c_str(),(uint32_t)source.size(),hash);assert(!blob);
}
struct reader_job {const std::string* source;ID3DBlob* expected;volatile LONG stop;};
static DWORD WINAPI reader(void* opaque) {
    auto* j=(reader_job*)opaque;auto hash=ld_cache_hash(j->source->data(),j->source->size(),1469598103934665603ull);
    while(!InterlockedCompareExchange(&j->stop,0,0)) {
        ID3DBlob* b=ld_shpre_find('P',j->source->c_str(),(uint32_t)j->source->size(),hash);
        if(b){assert(b->lpVtbl->GetBufferSize(b)==j->expected->lpVtbl->GetBufferSize(j->expected));
            assert(!memcmp(b->lpVtbl->GetBufferPointer(b),j->expected->lpVtbl->GetBufferPointer(j->expected),b->lpVtbl->GetBufferSize(b)));RELEASE(b);}
        SwitchToThread();
    }return 0;
}
static void preload_tests(const std::string& directory) {
    CreateDirectoryA(directory.c_str(),nullptr);
    const std::string basic="float4 main():SV_Target{return float4(.2,.4,.6,1);}";
    ID3DBlob* blob=compile(basic.c_str(),"ps_5_0");
    std::vector<std::string> sources;for(unsigned i=0;i<24;i++) {
        sources.push_back(basic+"\n/*"+std::to_string(i)+std::string(60000,' ')+"*/");fixture(directory,sources.back(),blob);
    }
    _putenv_s("LD_SHADER_PRELOAD","0");ld_shpre_start(directory.c_str(),magic,2,4*1024*1024);assert(!s_shpre.buckets);
    _putenv_s("LD_SHADER_PRELOAD","1");_putenv_s("LD_SHADER_PRELOAD_MB","64");
    ld_shpre_start(directory.c_str(),magic,2,4*1024*1024);
    reader_job job={&sources[0],blob,0};HANDLE workers[4];for(auto& h:workers)h=CreateThread(nullptr,0,reader,&job,0,nullptr);
    wait_preload();InterlockedExchange(&job.stop,1);assert(WaitForMultipleObjects(4,workers,TRUE,15000)==WAIT_OBJECT_0);for(auto h:workers)CloseHandle(h);
    assert(s_shpre.from_files==sources.size()&&s_shpre.count==sources.size());
    for(const auto& source:sources)check_blob(source,blob);ld_shpre_stop();
    _putenv_s("LD_SHADER_PRELOAD", "");
    ld_shpre_start(directory.c_str(),magic,2,4*1024*1024);wait_preload();check_blob(sources[0],blob);ld_shpre_stop();
    ld_shpre_start(directory.c_str(),magic,2,4*1024*1024);wait_preload();assert(s_shpre.from_pack==sources.size()&&!s_shpre.from_files);check_blob(sources[0],blob);ld_shpre_stop();
    const auto path=directory+"/ld_shader_pack_v1.bin";
    FILE* f=fopen(path.c_str(),"rb+");assert(f);assert(!fseek(f,-1,SEEK_END));int c=fgetc(f);assert(!fseek(f,-1,SEEK_END));fputc(c^1,f);fclose(f);
    ld_shpre_start(directory.c_str(),magic,2,4*1024*1024);wait_preload();assert(s_shpre.rejected&&s_shpre.from_files);for(const auto& source:sources)check_blob(source,blob);ld_shpre_stop();
    f=fopen(path.c_str(),"wb");assert(f);fwrite("bad",1,3,f);fclose(f);
    ld_shpre_start(directory.c_str(),magic,2,4*1024*1024);wait_preload();assert(s_shpre.rejected&&s_shpre.count==sources.size());ld_shpre_stop();
    ld_shpre_start(directory.c_str(),magic,3,4*1024*1024);wait_preload();assert(!s_shpre.count&&s_shpre.rejected);ld_shpre_stop();
    _putenv_s("LD_SHADER_PRELOAD_MB","1");ld_shpre_start(directory.c_str(),magic,2,4*1024*1024);wait_preload();assert(s_shpre.limited&&s_shpre.bytes<=1024*1024&&s_shpre.count<sources.size());ld_shpre_stop();
    DWORD handles_before=0,handles_after=0;GetProcessHandleCount(GetCurrentProcess(),&handles_before);
    for(unsigned i=0;i<20;i++){ld_shpre_start(directory.c_str(),magic,2,4*1024*1024);ld_shpre_stop();assert(!s_shpre.buckets&&!s_shpre.thread);}
    GetProcessHandleCount(GetCurrentProcess(),&handles_after);assert(handles_after<=handles_before+2);
    RELEASE(blob);printf("preload: round-trip bytes/source/stage, pack corruption/truncation/version, budget, four concurrent readers, 20 cancellation cycles passed\n");
}
static D3D12_GRAPHICS_PIPELINE_STATE_DESC description(ID3D12RootSignature* root,ID3DBlob* vs,ID3DBlob* ps) {
    D3D12_GRAPHICS_PIPELINE_STATE_DESC p={};p.pRootSignature=root;
    p.VS={vs->lpVtbl->GetBufferPointer(vs),vs->lpVtbl->GetBufferSize(vs)};p.PS={ps->lpVtbl->GetBufferPointer(ps),ps->lpVtbl->GetBufferSize(ps)};
    auto& b=p.BlendState.RenderTarget[0];b.SrcBlend=b.SrcBlendAlpha=D3D12_BLEND_ONE;b.DestBlend=b.DestBlendAlpha=D3D12_BLEND_ZERO;
    b.BlendOp=b.BlendOpAlpha=D3D12_BLEND_OP_ADD;b.LogicOp=D3D12_LOGIC_OP_NOOP;b.RenderTargetWriteMask=D3D12_COLOR_WRITE_ENABLE_ALL;
    p.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID;p.RasterizerState.CullMode=D3D12_CULL_MODE_NONE;p.RasterizerState.DepthClipEnable=TRUE;
    p.DepthStencilState.DepthFunc=D3D12_COMPARISON_FUNC_ALWAYS;p.DepthStencilState.FrontFace={D3D12_STENCIL_OP_KEEP,D3D12_STENCIL_OP_KEEP,D3D12_STENCIL_OP_KEEP,D3D12_COMPARISON_FUNC_ALWAYS};p.DepthStencilState.BackFace=p.DepthStencilState.FrontFace;
    p.SampleMask=UINT_MAX;p.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;p.NumRenderTargets=1;p.RTVFormats[0]=DXGI_FORMAT_R8G8B8A8_UNORM;p.SampleDesc.Count=1;return p;
}
static void pipeline_tests(const std::string& directory,bool read_existing) {
    CreateDirectoryA(directory.c_str(),nullptr);
    ID3D12Device* device=nullptr;assert(SUCCEEDED(D3D12CreateDevice(nullptr,D3D_FEATURE_LEVEL_11_0,LD_CACHE_IID(IID_ID3D12Device),(void**)&device)));
    ID3DBlob *sig=nullptr,*errors=nullptr;D3D12_ROOT_SIGNATURE_DESC rd={};
    assert(SUCCEEDED(D3D12SerializeRootSignature(&rd,D3D_ROOT_SIGNATURE_VERSION_1,&sig,&errors)));RELEASE(errors);
    ID3D12RootSignature* root=nullptr;assert(SUCCEEDED(device->lpVtbl->CreateRootSignature(device,0,sig->lpVtbl->GetBufferPointer(sig),sig->lpVtbl->GetBufferSize(sig),LD_CACHE_IID(IID_ID3D12RootSignature),(void**)&root)));
    ID3DBlob* vs=compile("float4 main(uint i:SV_VertexID):SV_Position{return float4(i==1?3:-1,i==2?3:-1,0,1);}","vs_5_0");
    ID3DBlob* ps=compile("float4 main():SV_Target{return float4(.2,.4,.6,1);}","ps_5_0");
    auto pd=description(root,vs,ps);ID3D12PipelineState* pso=nullptr;
    _putenv_s("LD_PSO_LIBRARY","0");assert(SUCCEEDED(ld_plib_create(device,&pd,&pso))&&s_plib.last_hit==-1);RELEASE(pso);
    ld_plib_start(device,root,sig->lpVtbl->GetBufferPointer(sig),sig->lpVtbl->GetBufferSize(sig),directory.c_str());assert(!s_plib.library);
    _putenv_s("LD_PSO_LIBRARY","");_putenv_s("LD_PSO_LIBRARY_SAVE_MS","1000");
    ld_plib_start(device,root,sig->lpVtbl->GetBufferPointer(sig),sig->lpVtbl->GetBufferSize(sig),directory.c_str());assert(s_plib.library);
    assert(SUCCEEDED(ld_plib_create(device,&pd,&pso)));assert(s_plib.last_hit==(read_existing?1:0));RELEASE(pso);
    assert(SUCCEEDED(ld_plib_create(device,&pd,&pso))&&s_plib.last_hit==1);RELEASE(pso);
    WCHAR original[64],padded[64];assert(ld_plib_name(&pd,original));auto padding=pd;
    memset((unsigned char*)&padding.BlendState.RenderTarget[0]+offsetof(D3D12_RENDER_TARGET_BLEND_DESC,RenderTargetWriteMask)+1,0xa5,3);
    assert(ld_plib_name(&padding,padded)&&!wcscmp(original,padded));
    auto other=pd;other.BlendState.RenderTarget[0].RenderTargetWriteMask=7;
    assert(SUCCEEDED(ld_plib_create(device,&other,&pso)));RELEASE(pso);
    assert(SUCCEEDED(ld_plib_create(device,&other,&pso))&&s_plib.last_hit==1);RELEASE(pso);
    for(unsigned i=0;i<16;i++){other=pd;other.BlendState.RenderTarget[0].RenderTargetWriteMask=(UINT8)i;assert(SUCCEEDED(ld_plib_create(device,&other,&pso)));RELEASE(pso);}
    assert(WaitForSingleObject(s_plib.stop,1400)==WAIT_TIMEOUT); // allow the single periodic writer to serialize
    std::string path=s_plib.path;ld_plib_stop();assert(!s_plib.library&&!s_plib.backing&&!s_plib.thread);
    if(!read_existing){printf("pipeline: seed persisted; run a second process to verify reload\n");}
    else {
        FILE* f=fopen(path.c_str(),"wb");assert(f);fwrite("bad",1,3,f);fclose(f);
        ld_plib_start(device,root,sig->lpVtbl->GetBufferPointer(sig),sig->lpVtbl->GetBufferSize(sig),directory.c_str());assert(s_plib.library);
        assert(SUCCEEDED(ld_plib_create(device,&pd,&pso))&&s_plib.last_hit==0);RELEASE(pso);ld_plib_stop();
        ld_plib_start(device,root,sig->lpVtbl->GetBufferPointer(sig),sig->lpVtbl->GetBufferSize(sig),directory.c_str());assert(s_plib.library);s_plib.limit=1;
        other=pd;other.SampleMask=0x7fffffff;assert(SUCCEEDED(ld_plib_create(device,&other,&pso)));RELEASE(pso);assert(s_plib.store_limit&&!s_plib.stores);ld_plib_stop();
    }
    RELEASE(ps);RELEASE(vs);RELEASE(root);RELEASE(sig);RELEASE(device);
    printf("pipeline: real hardware PSO create/hit, identity/state/padding, writer/shutdown%s passed\n",read_existing?", cross-process reload/corruption/budget fallback":"");
}
int main(int argc,char** argv) {
    assert(argc==3);if(!strcmp(argv[1],"preload"))preload_tests(argv[2]);
    else pipeline_tests(argv[2],!strcmp(argv[1],"pipeline-read"));
}
