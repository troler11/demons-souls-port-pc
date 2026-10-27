// Run against HLSL extracted from the renderer by test_framegen_shader.py.
// The bounded search must return exactly the full search's motion vectors.
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
using Microsoft::WRL::ComPtr;

static void check(HRESULT hr) {
    if (FAILED(hr)) { std::fprintf(stderr, "D3D failure %08lX\n", (unsigned long)hr); std::exit(2); }
}

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    std::ifstream input(argv[1], std::ios::binary);
    const std::string source{std::istreambuf_iterator<char>(input), {}};
    if (source.empty()) return 2;
    ComPtr<ID3D11Device> dev;
    ComPtr<ID3D11DeviceContext> ctx;
    D3D_FEATURE_LEVEL feature;
    check(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
                           D3D11_SDK_VERSION, &dev, &feature, &ctx));
    for (int pass = 0; pass != 2; ++pass) {
        const unsigned w = pass ? 256 : 64, h = pass ? 192 : 48, gw = 16, gh = 12;
        const char* entry = pass ? "cs_refine" : "cs_coarse";
        ComPtr<ID3D11ComputeShader> shaders[2];
        for (int full = 0; full != 2; ++full) {
            const D3D_SHADER_MACRO macros[] = {{"FG_FULL_SEARCH", "1"}, {nullptr, nullptr}};
            ComPtr<ID3DBlob> blob, errors;
            HRESULT hr = D3DCompile(source.data(), source.size(), "framegen", full ? macros : nullptr,
                                   nullptr, entry, "cs_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &blob, &errors);
            if (FAILED(hr) && errors) std::fprintf(stderr, "%s\n", (const char*)errors->GetBufferPointer());
            check(hr);
            check(dev->CreateComputeShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &shaders[full]));
        }
        for (int pattern = 0; pattern != 5; ++pattern) {
            std::vector<float> a(w * h), b(w * h);
            auto value = [&](int x, int y) {
                x = std::clamp(x, 0, (int)w - 1); y = std::clamp(y, 0, (int)h - 1);
                if (pattern == 0) return 0.0f;
                if (pattern == 1) return 0.5f;
                if (pattern == 2) return ((x / 5 + y / 7) % 2) ? 0.8f : 0.1f;
                unsigned hash = (unsigned)x * 73856093u ^ (unsigned)y * 19349663u;
                return (float)(hash & 1023u) / 1023.0f;
            };
            for (unsigned y = 0; y < h; ++y) for (unsigned x = 0; x < w; ++x) {
                a[y * w + x] = value((int)x, (int)y);
                b[y * w + x] = value((int)x - (pattern == 3 ? 4 : 0), (int)y - (pattern == 3 ? 2 : 0));
                if (pattern == 4 && x > w / 3 && x < w / 2) b[y * w + x] *= 0.3f;
            }
            ComPtr<ID3D11Texture2D> textures[2];
            ComPtr<ID3D11ShaderResourceView> srvs[2];
            D3D11_TEXTURE2D_DESC td = {};
            td.Width = w; td.Height = h; td.MipLevels = td.ArraySize = 1;
            td.Format = DXGI_FORMAT_R32_FLOAT; td.SampleDesc.Count = 1;
            td.Usage = D3D11_USAGE_IMMUTABLE; td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            for (int i = 0; i != 2; ++i) {
                D3D11_SUBRESOURCE_DATA data = {i ? b.data() : a.data(), (UINT)(w * sizeof(float)), 0};
                check(dev->CreateTexture2D(&td, &data, &textures[i]));
                check(dev->CreateShaderResourceView(textures[i].Get(), nullptr, &srvs[i]));
            }
            std::vector<float> zero(gw * gh * 2);
            td.Width = gw; td.Height = gh; td.Format = DXGI_FORMAT_R32G32_FLOAT;
            D3D11_SUBRESOURCE_DATA zd = {zero.data(), (UINT)(gw * 2 * sizeof(float)), 0};
            ComPtr<ID3D11Texture2D> coarse;
            ComPtr<ID3D11ShaderResourceView> coarse_srv;
            check(dev->CreateTexture2D(&td, &zd, &coarse));
            check(dev->CreateShaderResourceView(coarse.Get(), nullptr, &coarse_srv));
            td.Usage = D3D11_USAGE_DEFAULT;
            td.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
            ComPtr<ID3D11Texture2D> output;
            ComPtr<ID3D11UnorderedAccessView> uav;
            check(dev->CreateTexture2D(&td, nullptr, &output));
            check(dev->CreateUnorderedAccessView(output.Get(), nullptr, &uav));
            td.Usage = D3D11_USAGE_STAGING; td.BindFlags = 0; td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            ComPtr<ID3D11Texture2D> readback;
            check(dev->CreateTexture2D(&td, nullptr, &readback));
            const int k[8] = {(int)gw, (int)gh, (int)w, (int)h, 0, 0, 0, 0};
            D3D11_BUFFER_DESC bd = {};
            bd.ByteWidth = sizeof k; bd.Usage = D3D11_USAGE_IMMUTABLE; bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            D3D11_SUBRESOURCE_DATA kd = {k, 0, 0};
            ComPtr<ID3D11Buffer> constants;
            check(dev->CreateBuffer(&bd, &kd, &constants));
            ID3D11Buffer* cb = constants.Get();
            ID3D11ShaderResourceView* inputs[3] = {srvs[0].Get(), srvs[1].Get(), coarse_srv.Get()};
            ctx->CSSetShaderResources(2, 3, inputs);
            ctx->CSSetConstantBuffers(0, 1, &cb);
            std::vector<float> results[2];
            for (int full = 0; full != 2; ++full) {
                ID3D11UnorderedAccessView* view = uav.Get();
                ctx->CSSetUnorderedAccessViews(2, 1, &view, nullptr);
                ctx->CSSetShader(shaders[full].Get(), nullptr, 0);
                ctx->Dispatch((gw + 7) / 8, (gh + 7) / 8, 1);
                view = nullptr; ctx->CSSetUnorderedAccessViews(2, 1, &view, nullptr);
                ctx->CopyResource(readback.Get(), output.Get());
                D3D11_MAPPED_SUBRESOURCE mapped;
                check(ctx->Map(readback.Get(), 0, D3D11_MAP_READ, 0, &mapped));
                results[full].resize(gw * gh * 2);
                for (unsigned y = 0; y < gh; ++y)
                    std::memcpy(results[full].data() + y * gw * 2, (const char*)mapped.pData + y * mapped.RowPitch,
                                gw * 2 * sizeof(float));
                ctx->Unmap(readback.Get(), 0);
            }
            if (results[0] != results[1]) {
                std::fprintf(stderr, "Motion mismatch: %s pattern %d\n", entry, pattern); return 1;
            }
            ctx->ClearState();
        }
    }
    // A brightness/shadow change with no valid motion must use the current
    // image instead of averaging unrelated frames into a visible trail.
    ComPtr<ID3DBlob> vs_blob, ps_blob;
    check(D3DCompile(source.data(), source.size(), "framegen", nullptr, nullptr, "vs", "vs_5_0",
                     D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &vs_blob, nullptr));
    check(D3DCompile(source.data(), source.size(), "framegen", nullptr, nullptr, "ps_interp", "ps_5_0",
                     D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &ps_blob, nullptr));
    ComPtr<ID3D11VertexShader> vs;
    ComPtr<ID3D11PixelShader> ps;
    check(dev->CreateVertexShader(vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), nullptr, &vs));
    check(dev->CreatePixelShader(ps_blob->GetBufferPointer(), ps_blob->GetBufferSize(), nullptr, &ps));
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = td.Height = 16; td.MipLevels = td.ArraySize = 1; td.SampleDesc.Count = 1;
    td.Format = DXGI_FORMAT_R32G32B32A32_FLOAT; td.Usage = D3D11_USAGE_IMMUTABLE;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    ComPtr<ID3D11Texture2D> frames[2];
    ComPtr<ID3D11ShaderResourceView> frame_srvs[2];
    for (int i = 0; i != 2; ++i) {
        std::vector<float> data(16 * 16 * 4, i ? 0.8f : 0.1f);
        for (unsigned p = 3; p < data.size(); p += 4) data[p] = 1.0f;
        D3D11_SUBRESOURCE_DATA init = {data.data(), 16 * 4 * sizeof(float), 0};
        check(dev->CreateTexture2D(&td, &init, &frames[i]));
        check(dev->CreateShaderResourceView(frames[i].Get(), nullptr, &frame_srvs[i]));
    }
    td.Width = td.Height = 1; td.Format = DXGI_FORMAT_R32G32_FLOAT;
    const float no_motion[2] = {0, 0};
    D3D11_SUBRESOURCE_DATA motion_data = {no_motion, sizeof no_motion, 0};
    ComPtr<ID3D11Texture2D> motion;
    ComPtr<ID3D11ShaderResourceView> motion_srv;
    check(dev->CreateTexture2D(&td, &motion_data, &motion));
    check(dev->CreateShaderResourceView(motion.Get(), nullptr, &motion_srv));
    td.Width = td.Height = 16; td.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11Texture2D> target;
    ComPtr<ID3D11RenderTargetView> rtv;
    check(dev->CreateTexture2D(&td, nullptr, &target));
    check(dev->CreateRenderTargetView(target.Get(), nullptr, &rtv));
    td.Usage = D3D11_USAGE_STAGING; td.BindFlags = 0; td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> readback;
    check(dev->CreateTexture2D(&td, nullptr, &readback));
    const int k[8] = {16, 16, 1, 1, 0, 0, 0, 0};
    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = sizeof k; bd.Usage = D3D11_USAGE_IMMUTABLE; bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    D3D11_SUBRESOURCE_DATA init = {k, 0, 0};
    ComPtr<ID3D11Buffer> cb;
    check(dev->CreateBuffer(&bd, &init, &cb));
    D3D11_SAMPLER_DESC sampler_desc = {};
    sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampler_desc.AddressU = sampler_desc.AddressV = sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;
    ComPtr<ID3D11SamplerState> sampler;
    check(dev->CreateSamplerState(&sampler_desc, &sampler));
    D3D11_VIEWPORT viewport = {0, 0, 16, 16, 0, 1};
    ID3D11RenderTargetView* output = rtv.Get();
    ID3D11Buffer* constants = cb.Get();
    ID3D11SamplerState* sampling = sampler.Get();
    ID3D11ShaderResourceView* inputs[5] = {frame_srvs[0].Get(), frame_srvs[1].Get(), nullptr, nullptr, motion_srv.Get()};
    ctx->OMSetRenderTargets(1, &output, nullptr);
    ctx->RSSetViewports(1, &viewport);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->VSSetShader(vs.Get(), nullptr, 0); ctx->PSSetShader(ps.Get(), nullptr, 0);
    ctx->PSSetShaderResources(0, 5, inputs);
    ctx->PSSetConstantBuffers(0, 1, &constants); ctx->PSSetSamplers(0, 1, &sampling);
    ctx->Draw(3, 0);
    ctx->ClearState();
    ctx->CopyResource(readback.Get(), target.Get());
    D3D11_MAPPED_SUBRESOURCE mapped;
    check(ctx->Map(readback.Get(), 0, D3D11_MAP_READ, 0, &mapped));
    for (unsigned y = 0; y != 16; ++y) {
        const float* row = (const float*)((const char*)mapped.pData + y * mapped.RowPitch);
        for (unsigned x = 0; x != 16; ++x) for (unsigned c = 0; c != 4; ++c) {
            const float expected = c == 3 ? 1.0f : 0.8f;
            if (row[x * 4 + c] < expected - 0.00001f || row[x * 4 + c] > expected + 0.00001f) {
                std::fprintf(stderr, "Uncertain shadow match was blended\n"); return 1;
            }
        }
    }
    ctx->Unmap(readback.Get(), 0);
    std::puts("PASS: exact coarse/refine vectors on five fixtures; uncertain shadow match uses current image");
}
