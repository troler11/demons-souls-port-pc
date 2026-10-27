#define CINTERFACE
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <initguid.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <cassert>
#include <string>
#include <vector>
#include "rsx_shader_preload.h"
#include "rsx_pipeline_library.h"
static HANDLE gate_entered,gate_release;
static volatile LONG gate_on,create_calls,fail_on,delay_ms;
static HANDLE read_entered;
static volatile LONG read_gate;
static void read_step(HANDLE stop){if(InterlockedCompareExchange(&read_gate,0,0)){SetEvent(read_entered);assert(WaitForSingleObject(stop,15000)==WAIT_OBJECT_0);}}
static HRESULT create_test(ID3D12Device* device,const D3D12_GRAPHICS_PIPELINE_STATE_DESC* p,ID3D12PipelineState** out){
    InterlockedIncrement(&create_calls);
    LONG delay=InterlockedCompareExchange(&delay_ms,0,0);if(delay)Sleep((DWORD)delay);
    if(InterlockedCompareExchange(&fail_on,0,0)){*out=nullptr;return E_FAIL;}
    if(InterlockedCompareExchange(&gate_on,0,0)){SetEvent(gate_entered);assert(WaitForSingleObject(gate_release,15000)==WAIT_OBJECT_0);}
    return device->lpVtbl->CreateGraphicsPipelineState(device,p,LD_CACHE_IID(IID_ID3D12PipelineState),(void**)out);
}
#define LD_PW_CREATE(device,desc,out) create_test(device,desc,out)
#define LD_PW_READ_STEP(stop) read_step(stop)
#include "rsx_pso_warmup.h"
#define RELEASE(p) do{if(p){(p)->lpVtbl->Release(p);(p)=nullptr;}}while(0)
static ID3DBlob* compile(const char* s,const char* target){ID3DBlob*b=nullptr,*e=nullptr;auto hr=D3DCompile(s,strlen(s),nullptr,nullptr,nullptr,"main",target,0,0,&b,&e);if(e)fprintf(stderr,"%s\n",(char*)e->lpVtbl->GetBufferPointer(e));RELEASE(e);assert(SUCCEEDED(hr)&&b);return b;}
static D3D12_GRAPHICS_PIPELINE_STATE_DESC description(ID3D12RootSignature* root,ID3DBlob* vs,ID3DBlob* ps){
    D3D12_GRAPHICS_PIPELINE_STATE_DESC p={};p.pRootSignature=root;
    p.VS={vs->lpVtbl->GetBufferPointer(vs),vs->lpVtbl->GetBufferSize(vs)};p.PS={ps->lpVtbl->GetBufferPointer(ps),ps->lpVtbl->GetBufferSize(ps)};
    for(auto&b:p.BlendState.RenderTarget){b.SrcBlend=b.SrcBlendAlpha=D3D12_BLEND_ONE;b.DestBlend=b.DestBlendAlpha=D3D12_BLEND_ZERO;b.BlendOp=b.BlendOpAlpha=D3D12_BLEND_OP_ADD;b.LogicOp=D3D12_LOGIC_OP_NOOP;b.RenderTargetWriteMask=15;}
    p.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID;p.RasterizerState.CullMode=D3D12_CULL_MODE_NONE;p.RasterizerState.DepthClipEnable=TRUE;
    p.DepthStencilState.DepthFunc=D3D12_COMPARISON_FUNC_ALWAYS;p.DepthStencilState.FrontFace={D3D12_STENCIL_OP_KEEP,D3D12_STENCIL_OP_KEEP,D3D12_STENCIL_OP_KEEP,D3D12_COMPARISON_FUNC_ALWAYS};p.DepthStencilState.BackFace=p.DepthStencilState.FrontFace;
    p.SampleMask=UINT_MAX;p.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;p.NumRenderTargets=1;p.RTVFormats[0]=DXGI_FORMAT_R8G8B8A8_UNORM;p.SampleDesc.Count=1;return p;
}
static std::vector<unsigned char> read(const std::string& path){FILE*f=fopen(path.c_str(),"rb");assert(f);fseek(f,0,SEEK_END);auto n=ftell(f);rewind(f);std::vector<unsigned char>b(n);assert(fread(b.data(),1,b.size(),f)==b.size());fclose(f);return b;}
static void write(const std::string&path,const std::vector<unsigned char>&b){FILE*f=fopen(path.c_str(),"wb");assert(f);assert(fwrite(b.data(),1,b.size(),f)==b.size());assert(!fclose(f));}
static void wait_done(){assert(s_pw.done&&WaitForSingleObject(s_pw.done,15000)==WAIT_OBJECT_0);}
static ID3D12PipelineState* fresh(ID3D12Device*d,const D3D12_GRAPHICS_PIPELINE_STATE_DESC&p){ID3D12PipelineState*s=nullptr;assert(SUCCEEDED(d->lpVtbl->CreateGraphicsPipelineState(d,&p,LD_CACHE_IID(IID_ID3D12PipelineState),(void**)&s))) ;assert(s);return s;}
static void codec(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& base){
    auto p=base;p.BlendState.AlphaToCoverageEnable=1;p.BlendState.IndependentBlendEnable=1;
    for(unsigned i=0;i<8;i++){auto& b=p.BlendState.RenderTarget[i];b.BlendEnable=i&1;b.LogicOpEnable=(i>>1)&1;b.RenderTargetWriteMask=(UINT8)i;b.DestBlend=D3D12_BLEND_SRC_ALPHA;b.BlendOpAlpha=D3D12_BLEND_OP_MAX;}
    p.RasterizerState.DepthBias=-7;p.RasterizerState.DepthBiasClamp=.125f;p.RasterizerState.SlopeScaledDepthBias=-.5f;p.RasterizerState.FrontCounterClockwise=1;p.RasterizerState.MultisampleEnable=1;p.RasterizerState.AntialiasedLineEnable=1;p.RasterizerState.ForcedSampleCount=4;p.RasterizerState.ConservativeRaster=D3D12_CONSERVATIVE_RASTERIZATION_MODE_ON;
    p.DepthStencilState.DepthEnable=1;p.DepthStencilState.DepthWriteMask=D3D12_DEPTH_WRITE_MASK_ALL;p.DepthStencilState.StencilEnable=1;p.DepthStencilState.StencilReadMask=0xa5;p.DepthStencilState.StencilWriteMask=0x5a;p.DepthStencilState.BackFace.StencilPassOp=D3D12_STENCIL_OP_REPLACE;
    p.IBStripCutValue=D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_0xFFFFFFFF;p.SampleMask=0xf00ff00f;p.NumRenderTargets=8;
    for(auto&f:p.RTVFormats)f=DXGI_FORMAT_R16G16B16A16_FLOAT;p.DSVFormat=DXGI_FORMAT_D24_UNORM_S8_UINT;p.SampleDesc={4,3};p.NodeMask=1;p.Flags=D3D12_PIPELINE_STATE_FLAG_TOOL_DEBUG;
    p.GS=p.HS=p.DS=p.VS;
    D3D12_INPUT_ELEMENT_DESC input[2]={{"POSITION",2,DXGI_FORMAT_R32G32B32_FLOAT,1,4,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},{"TEXCOORD",3,DXGI_FORMAT_R32G32_FLOAT,2,16,D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA,7}};p.InputLayout={input,2};
    uint32_t n=0;auto*wire=ld_pw_encode(&p,&n);assert(wire);auto*e=ld_pw_decode(wire,n);assert(e);
    assert(ld_plib_description_hash(&p,123)==ld_plib_description_hash(&e->desc,123));
    assert(e->desc.InputLayout.pInputElementDescs[1].SemanticName!=input[1].SemanticName);
    uint32_t n2;auto*encoded=ld_pw_encode(&e->desc,&n2);assert(n==n2&&!memcmp(e->wire,encoded,n));free(encoded);ld_pw_free(e);
    p=base;p.InputLayout.NumElements=33;assert(!ld_pw_encode(&p,&n));p=base;p.VS.pShaderBytecode=nullptr;assert(!ld_pw_encode(&p,&n));
    p=base;wire=ld_pw_encode(&p,&n);assert(wire);ld_pw_wire h;memcpy(&h,wire,sizeof h);h.shaders[0]=UINT_MAX;memcpy(wire,&h,sizeof h);assert(!ld_pw_decode(wire,n));
    wire=ld_pw_encode(&base,&n);assert(wire);assert(!ld_pw_decode(wire,sizeof(ld_pw_wire)-1));
}
int main(int argc,char**argv){
    assert(argc==3);std::string dir=argv[2];CreateDirectoryA(dir.c_str(),nullptr);bool loading=!strcmp(argv[1],"read");
    ID3D12Device*d=nullptr;assert(SUCCEEDED(D3D12CreateDevice(nullptr,D3D_FEATURE_LEVEL_11_0,LD_CACHE_IID(IID_ID3D12Device),(void**)&d)));
    ID3DBlob* sig=nullptr,*err=nullptr;D3D12_ROOT_SIGNATURE_DESC rd={};assert(SUCCEEDED(D3D12SerializeRootSignature(&rd,D3D_ROOT_SIGNATURE_VERSION_1,&sig,&err)));RELEASE(err);
    ID3D12RootSignature*root=nullptr;assert(SUCCEEDED(d->lpVtbl->CreateRootSignature(d,0,sig->lpVtbl->GetBufferPointer(sig),sig->lpVtbl->GetBufferSize(sig),LD_CACHE_IID(IID_ID3D12RootSignature),(void**)&root)));
    auto*vs=compile("float4 main(uint i:SV_VertexID):SV_Position{return float4(i==1?3:-1,i==2?3:-1,0,1);}","vs_5_0");auto*ps=compile("float4 main():SV_Target{return float4(.2,.4,.6,1);}","ps_5_0");auto p=description(root,vs,ps);
    _putenv_s("LD_PSO_LIBRARY","1");ld_plib_start(d,root,sig->lpVtbl->GetBufferPointer(sig),sig->lpVtbl->GetBufferSize(sig),dir.c_str());assert(s_plib.library);
    _putenv_s("LD_PSO_WARMUP","");_putenv_s("LD_PSO_WARMUP_RECORD","");ld_pw_start(d,dir.c_str());assert(!s_pw.device);
    _putenv_s("LD_PSO_WARMUP","0");_putenv_s("LD_PSO_WARMUP_RECORD",loading?"0":"1");
    if(!loading){
        ld_pw_start(d,dir.c_str());wait_done();assert(!create_calls);codec(p);
        for(unsigned i=0;i<12;i++){auto a=p;a.SampleMask=UINT_MAX-i;auto*s=fresh(d,a);ld_pw_remember(&a,s);RELEASE(s);}
        assert(s_pw.count==12&&!s_pw.created);ld_pw_stop();
        puts("warmup write: 12 real PSOs, record-only has zero background creates; complete codec/state/shader/input layout passed");
    }else{
        _putenv_s("LD_PSO_WARMUP","1");ld_pw_start(d,dir.c_str());wait_done();assert(s_pw.count==12&&s_pw.created==12&&!s_pw.failed);
        ID3D12PipelineState* held=nullptr;
        for(unsigned i=0;i<12;i++){auto a=p;a.SampleMask=UINT_MAX-i;auto*s=ld_pw_find(&a);assert(s);if(!i)held=s;else RELEASE(s);}
        auto different=p;different.SampleMask=0x111;assert(!ld_pw_find(&different));
        auto padding=p;memset((unsigned char*)&padding.BlendState.RenderTarget[0]+offsetof(D3D12_RENDER_TARGET_BLEND_DESC,RenderTargetWriteMask)+1,0xa5,3);auto*s=ld_pw_find(&padding);assert(s==held);RELEASE(s);
        // Force the same lookup key for a different complete description.
        WCHAR collision[64];assert(ld_plib_name(&different,collision));
        AcquireSRWLockExclusive(&s_pw.lock);auto*e=s_pw.entries[0];auto**link=&s_pw.buckets[ld_pw_bucket(e->name)];while(*link!=e)link=&(*link)->next;*link=e->next;
        wcscpy(e->name,collision);e->next=s_pw.buckets[ld_pw_bucket(collision)];s_pw.buckets[ld_pw_bucket(collision)]=e;ReleaseSRWLockExclusive(&s_pw.lock);
        assert(!ld_pw_find(&different));
        std::string path=s_pw.path;ld_pw_stop();ID3DBlob*cache=nullptr;assert(SUCCEEDED(held->lpVtbl->GetCachedBlob(held,&cache)));RELEASE(cache);RELEASE(held);
        auto original=read(path);auto bad=original;bad.back()^=1;write(path,bad);ld_pw_start(d,dir.c_str());wait_done();assert(s_pw.rejected&&s_pw.count==11);ld_pw_stop();
        bad.assign(original.begin(),original.begin()+sizeof(ld_pw_header)-1);write(path,bad);ld_pw_start(d,dir.c_str());wait_done();assert(s_pw.rejected&&!s_pw.count);ld_pw_stop();
        bad=original;((ld_pw_header*)bad.data())->version=99;write(path,bad);ld_pw_start(d,dir.c_str());wait_done();assert(s_pw.rejected&&!s_pw.count);ld_pw_stop();
        bad=original;((ld_pw_header*)bad.data())->root_hash[0]^=1;write(path,bad);ld_pw_start(d,dir.c_str());wait_done();assert(s_pw.rejected&&!s_pw.count);ld_pw_stop();write(path,original);
        _putenv_s("LD_PSO_WARMUP_MAX","1");_putenv_s("LD_PSO_WARMUP_RECORD","1");ld_pw_start(d,dir.c_str());wait_done();assert(s_pw.count==1&&s_pw.limited);
        AcquireSRWLockExclusive(&s_pw.lock);s_pw.generation++;ReleaseSRWLockExclusive(&s_pw.lock);
        ld_pw_stop();assert(read(path)==original);_putenv_s("LD_PSO_WARMUP_MAX","");
        gate_entered=CreateEventA(nullptr,TRUE,FALSE,nullptr);gate_release=CreateEventA(nullptr,TRUE,FALSE,nullptr);InterlockedExchange(&gate_on,1);_putenv_s("LD_PSO_WARMUP_RECORD","1");
        ld_pw_start(d,dir.c_str());assert(WaitForSingleObject(gate_entered,15000)==WAIT_OBJECT_0);assert(!ld_pw_find(&p));
        auto*normal=fresh(d,p);ld_pw_remember(&p,normal);auto*ready=ld_pw_find(&p);assert(ready==normal);RELEASE(ready);
        SetEvent(gate_release);wait_done();assert(s_pw.duplicates>=1&&s_pw.adopted>=1);ld_pw_stop();RELEASE(normal);InterlockedExchange(&gate_on,0);CloseHandle(gate_entered);CloseHandle(gate_release);
        InterlockedExchange(&fail_on,1);ld_pw_start(d,dir.c_str());wait_done();assert(s_pw.failed==12&&!ld_pw_find(&p));InterlockedExchange(&fail_on,0);
        normal=fresh(d,p);ld_pw_remember(&p,normal);ready=ld_pw_find(&p);assert(ready==normal);RELEASE(ready);ld_pw_stop();RELEASE(normal);
        read_entered=CreateEventA(nullptr,TRUE,FALSE,nullptr);InterlockedExchange(&read_gate,1);
        ld_pw_start(d,dir.c_str());assert(WaitForSingleObject(read_entered,15000)==WAIT_OBJECT_0);
        different=p;different.SampleMask=0x222;normal=fresh(d,different);ld_pw_remember(&different,normal);
        ld_pw_stop();RELEASE(normal);InterlockedExchange(&read_gate,0);CloseHandle(read_entered);assert(read(path)==original);
        _putenv_s("LD_PSO_WARMUP_TIME_MS","100");_putenv_s("LD_PSO_WARMUP_RECORD","0");InterlockedExchange(&delay_ms,100);
        ld_pw_start(d,dir.c_str());wait_done();assert(s_pw.timed_out&&s_pw.created>=1&&s_pw.created<12);ld_pw_stop();InterlockedExchange(&delay_ms,0);_putenv_s("LD_PSO_WARMUP_TIME_MS","");
        DWORD handles_before,handles_after;GetProcessHandleCount(GetCurrentProcess(),&handles_before);
        for(unsigned i=0;i<12;i++){ld_pw_start(d,dir.c_str());ld_pw_stop();assert(!s_pw.device&&!s_pw.thread&&!s_pw.root);}
        GetProcessHandleCount(GetCurrentProcess(),&handles_after);assert(handles_after<=handles_before+4);
        puts("warmup read: cross-process real PSOs, exact identity/padding, independent refs, corruption/truncation/version/root/budget fallback, pending-render race, cancellation/handles passed");
    }
    ld_pw_stop();ld_plib_stop();RELEASE(ps);RELEASE(vs);RELEASE(root);RELEASE(sig);RELEASE(d);return 0;
}
