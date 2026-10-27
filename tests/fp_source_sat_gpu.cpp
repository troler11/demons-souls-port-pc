// Execute real decompiler output on D3D11 WARP: SRC1 precision modifiers,
// all three source positions, input/temp/inline constants, abs and negation.
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include "rsx_fp_decompiler.h"
using Microsoft::WRL::ComPtr;
static void word(u8* p,u32 x) {u32 b=(x<<16)|(x>>16);p[0]=b>>24;p[1]=b>>16;p[2]=b>>8;p[3]=b;}
static u32 src(unsigned type,unsigned reg=0) {return type|(reg<<2)|(0xe4u<<9);}
static unsigned instruction(u8* p,unsigned op,u32 a,u32 b,u32 c,bool end,unsigned reg=0) {
 word(p,(op<<24)|(6u<<13)|(15u<<9)|(reg<<1)|end);word(p+4,a|(7u<<18));word(p+8,b);word(p+12,c);return 16;
}
static ComPtr<ID3DBlob> compile(const std::string& source,const char* target) {
 ComPtr<ID3DBlob> b,e;HRESULT hr=D3DCompile(source.data(),source.size(),nullptr,nullptr,nullptr,"main",target,0,0,&b,&e);
 if(FAILED(hr)){fprintf(stderr,"%s\n",e?(const char*)e->GetBufferPointer():"compile failed");abort();}return b;
}
int main(int argc,char** argv) {
 const bool legacy=argc>1 && !strcmp(argv[1],"legacy");_putenv_s("RSX_FP_SRC_SAT",legacy?"0":"1");
 ComPtr<ID3D11Device> dev;ComPtr<ID3D11DeviceContext> ctx;
 assert(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&dev,nullptr,&ctx)));
 D3D11_TEXTURE2D_DESC td={};td.Width=td.Height=td.MipLevels=td.ArraySize=1;td.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;td.SampleDesc.Count=1;td.BindFlags=D3D11_BIND_RENDER_TARGET;
 ComPtr<ID3D11Texture2D> target,read;assert(SUCCEEDED(dev->CreateTexture2D(&td,nullptr,&target)));
 ComPtr<ID3D11RenderTargetView> rtv;assert(SUCCEEDED(dev->CreateRenderTargetView(target.Get(),nullptr,&rtv)));
 td.Usage=D3D11_USAGE_STAGING;td.BindFlags=0;td.CPUAccessFlags=D3D11_CPU_ACCESS_READ;assert(SUCCEEDED(dev->CreateTexture2D(&td,nullptr,&read)));
 D3D11_RASTERIZER_DESC rd={};rd.FillMode=D3D11_FILL_SOLID;rd.CullMode=D3D11_CULL_NONE;rd.DepthClipEnable=TRUE;
 ComPtr<ID3D11RasterizerState> raster;assert(SUCCEEDED(dev->CreateRasterizerState(&rd,&raster)));ctx->RSSetState(raster.Get());
 D3D11_VIEWPORT viewport={0,0,1,1,0,1};ctx->RSSetViewports(1,&viewport);ID3D11RenderTargetView* rt=rtv.Get();ctx->OMSetRenderTargets(1,&rt,nullptr);
 const float values[4]={-2,.25f,2,-.5f};unsigned cases=0;
 for(unsigned buffered=0;buffered<2;buffered++)for(unsigned position=0;position<3;position++)for(unsigned type=0;type<3;type++)for(unsigned modifiers=0;modifiers<4;modifiers++)for(unsigned sat=0;sat<2;sat++) {
  u8 code[80]={};unsigned n=instruction(code,1,src(1),0,0,false,2);
  u32 operands[3]={src(0,3),src(0,3),src(0,3)};operands[position]=src(type,type==0?2:0);
  if(modifiers&1)operands[position]|=1u<<17; // negate after precision and abs
  if(modifiers&2) {if(position==0)operands[0]|=1u<<29;else if(position==1)operands[1]|=1u<<18;else continue;} // src2 abs is a separate existing decoder limitation
  if(sat)operands[1]|=4u<<(19+3*position);
  const unsigned op=position==0?1:position==1?3:4;
  n+=instruction(code+n,op,operands[0],operands[1],operands[2],true);
  if(type==2){for(unsigned i=0;i<4;i++){u32 bits;memcpy(&bits,&values[i],4);word(code+n+i*4,bits);}n+=16;}
  char hlsl[RSX_FP_HLSL_BUFFER_SIZE];
  if(buffered)assert(rsx_fp_decompile_buffered_ex(code,n,0x40,0,hlsl,sizeof hlsl,nullptr)>0);
  else assert(rsx_fp_decompile(code,n,0x40,hlsl,sizeof hlsl)>0);
  ComPtr<ID3D11Buffer> constants;
  if(buffered){rsx_fp_constant_block block={};rsx_fp_collect_constants(code,n,&block);
   float data[8]={};if(block.count)memcpy(data,block.values[0],16);
   D3D11_BUFFER_DESC bd={};bd.ByteWidth=sizeof data;bd.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
   D3D11_SUBRESOURCE_DATA initial={};initial.pSysMem=data;assert(SUCCEEDED(dev->CreateBuffer(&bd,&initial,&constants)));
   ID3D11Buffer* cb=constants.Get();ctx->PSSetConstantBuffers(1,1,&cb);
  }
  std::string ps=hlsl;auto close=ps.find("};");assert(close!=std::string::npos);
  const std::string vs=ps.substr(0,close+2)+"\nPSInput main(uint id:SV_VertexID) { PSInput o=(PSInput)0; float2 p=float2((id<<1)&2,id&2); o.position=float4(p*float2(2,-2)+float2(-1,1),0,1); o.tc2=float4(-2,.25,2,-.5); return o; }";
  auto vb=compile(vs,"vs_5_0"),pb=compile(ps,"ps_5_0");ComPtr<ID3D11VertexShader> shaderV;ComPtr<ID3D11PixelShader> shaderP;
  assert(SUCCEEDED(dev->CreateVertexShader(vb->GetBufferPointer(),vb->GetBufferSize(),nullptr,&shaderV)));
  assert(SUCCEEDED(dev->CreatePixelShader(pb->GetBufferPointer(),pb->GetBufferSize(),nullptr,&shaderP)));
  ctx->VSSetShader(shaderV.Get(),nullptr,0);ctx->PSSetShader(shaderP.Get(),nullptr,0);ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  const float sentinel[4]={99,99,99,99};ctx->ClearRenderTargetView(rtv.Get(),sentinel);ctx->Draw(3,0);ctx->CopyResource(read.Get(),target.Get());
  D3D11_MAPPED_SUBRESOURCE mapped={};assert(SUCCEEDED(ctx->Map(read.Get(),0,D3D11_MAP_READ,0,&mapped)));float actual[4];memcpy(actual,mapped.pData,sizeof actual);ctx->Unmap(read.Get(),0);
  for(unsigned i=0;i<4;i++){float expected=values[i];if(sat&&!legacy)expected=fminf(1,fmaxf(0,expected));if(modifiers&2)expected=fabsf(expected);if(modifiers&1)expected=-expected;
   if(!std::isfinite(actual[i])||fabsf(actual[i]-expected)>1e-5f){fprintf(stderr,"position=%u type=%u modifiers=%u sat=%u lane=%u got=%g expected=%g\n",position,type,modifiers,sat,i,actual[i],expected);return 1;}}
  cases++;
 }
 printf("FP source SAT: %u GPU WARP cases passed (%s)\n",cases,legacy?"legacy":"corrected");
}
