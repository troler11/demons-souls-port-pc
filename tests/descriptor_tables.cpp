// The lookup/table functions are extracted from rsx_live_draw.c by the runner.
// Mock heaps record every copied value so an independent oracle can check
// reuse, source mutations and ring lifetimes without relying on a hash match.
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <array>
#include "rsx_descriptor_index.h"
using u32=uint32_t;using u64=uint64_t;using UINT=unsigned;
struct D3D12_CPU_DESCRIPTOR_HANDLE {size_t ptr;};
struct D3D12_GPU_DESCRIPTOR_HANDLE {uint64_t ptr;};
enum {D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER};
struct Heap;
struct HeapVtable {
    void (*GetCPUDescriptorHandleForHeapStart)(Heap*,D3D12_CPU_DESCRIPTOR_HANDLE*);
    void (*GetGPUDescriptorHandleForHeapStart)(Heap*,D3D12_GPU_DESCRIPTOR_HANDLE*);
};
struct Heap {HeapVtable* lpVtbl;std::vector<u64> bytes;};
struct Device;
struct DeviceVtable {void (*CopyDescriptors)(Device*,UINT,const D3D12_CPU_DESCRIPTOR_HANDLE*,const UINT*,UINT,const D3D12_CPU_DESCRIPTOR_HANDLE*,const UINT*,int);};
struct Device {DeviceVtable* lpVtbl;};
constexpr u32 SRV_RING_TABLES=8192, SRV_TABLE_SIZE=16, SMP_RING_TABLES=128, SMP_TABLE_SIZE=16;
static u64 copies;
static void cpu(Heap* h,D3D12_CPU_DESCRIPTOR_HANDLE* out){out->ptr=(size_t)h->bytes.data();}
static void gpu(Heap* h,D3D12_GPU_DESCRIPTOR_HANDLE* out){out->ptr=(uint64_t)h->bytes.data();}
static void copy(Device*,UINT n,const D3D12_CPU_DESCRIPTOR_HANDLE* dst,const UINT* count,UINT sources,const D3D12_CPU_DESCRIPTOR_HANDLE* src,const UINT* sizes,int){
    assert(n==1&&*count==16&&sources==16&&!sizes);auto out=(u64*)dst->ptr;
    for(unsigned i=0;i<16;i++)out[i]=*(u64*)src[i].ptr;copies++;
}
static HeapVtable heap_vtable={cpu,gpu};static DeviceVtable device_vtable={copy};
static Device device={&device_vtable};
static Heap srv={&heap_vtable,std::vector<u64>(SRV_RING_TABLES*16+1)},smp={&heap_vtable,std::vector<u64>(SMP_RING_TABLES*16+1)};
static Heap srv_cpu_heap={&heap_vtable,std::vector<u64>(4096)},smp_cpu_heap={&heap_vtable,std::vector<u64>(4096)};
static struct {
    Device* dev=&device;Heap* srv_heap=&srv;Heap* smp_heap=&smp;
    Heap* srv_cpu_heap=&::srv_cpu_heap;Heap* smp_cpu_heap=&::smp_cpu_heap;
    u32 srv_step=8,smp_step=8,srv_ring_used=0,smp_ring_used=0;
    u32 smp_ring_slots[SMP_RING_TABLES][16]={};u64 fence_value=0;
} g;
#include "descriptor_functions.inc"

static void check(D3D12_GPU_DESCRIPTOR_HANDLE h,const u32* slots,Heap& source){
    assert(h.ptr);for(unsigned i=0;i<16;i++)assert(((u64*)h.ptr)[i]==source.bytes[slots[i]]);
}
static void recycle(){g.srv_ring_used=g.smp_ring_used=0;ld_descriptor_tables_reset();}
static u32 rng=0x81925ab3;
static u32 random32(){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
static void index_oracle(){
    ld_desc_entry entries[32];ld_desc_bucket buckets[64]={};ld_desc_index c={entries,buckets,32,64};
    std::vector<std::array<u32,16>> keys;
    // All keys deliberately map to the same initial bucket.
    for(u32 value=0;keys.size()<40;value++){
        std::array<u32,16> k={};k[0]=value;k[15]=value^0xa55a;
        if((ld_desc_hash(k.data())&63)==0)keys.push_back(k);
    }
    ld_desc_sync(&c,0,0);
    for(unsigned i=0;i<40;i++)ld_desc_insert(&c,keys[i].data(),i*16);
    assert(c.count==32);
    for(unsigned i=0;i<40;i++)assert(ld_desc_find(&c,keys[i].data())==(i<32?i*16:LD_DESC_NOT_FOUND));
    ld_desc_sync(&c,0,1);assert(ld_desc_find(&c,keys[0].data())==LD_DESC_NOT_FOUND);
    ld_desc_insert(&c,keys[0].data(),99);ld_desc_sync(&c,1,1);assert(ld_desc_find(&c,keys[0].data())==LD_DESC_NOT_FOUND);
    ld_desc_insert(&c,keys[0].data(),100);c.epoch=UINT32_MAX;ld_desc_clear(&c);
    assert(c.epoch==1&&!c.count&&ld_desc_find(&c,keys[0].data())==LD_DESC_NOT_FOUND);
}
int main(int argc,char** argv){
    assert(argc==2);bool indexed=!strcmp(argv[1],"1");bool no_reuse=!strcmp(argv[1],"no-srv-reuse");
    // Runner passes NAME= through CreateProcess: unlike _putenv_s(name,""),
    // this exercises actual empty variables in the child's environment.
    assert(!ld_debug_flag_on("RSX_D3D_DEBUG")&&!ld_debug_flag_on("RSX_D3D_GBV"));
    for(auto name:{"RSX_D3D_DEBUG","RSX_D3D_GBV"}){
        for(auto value:{"0","1","yes",""}){
            _putenv_s(name,value);assert(ld_debug_flag_on(name)==(*value&&*value!='0'));
        }
    }
    _putenv_s("LD_DESCRIPTOR_CACHE",indexed||no_reuse?"1":(!strcmp(argv[1],"unset")?"":"0"));
    _putenv_s("LD_SRV_REUSE",no_reuse?"0":"1");index_oracle();
    for(u32 i=0;i<4096;i++){srv_cpu_heap.bytes[i]=i*19+1;smp_cpu_heap.bytes[i]=i*23+2;}
    const u64 guard=0xdeadbeef12345678ull;srv.bytes.back()=smp.bytes.back()=guard;
    std::array<u32,16> a={},b={};b[15]=7;
    auto first=srv_table(a.data());auto middle=srv_table(b.data());(void)middle;
    auto repeat=srv_table(a.data());check(repeat,a.data(),srv_cpu_heap);
    assert((first.ptr==repeat.ptr)==indexed);
    std::array<u64,16> old;memcpy(old.data(),(void*)first.ptr,128);
    *(u64*)srv_cpu_w(0).ptr=9999;auto changed=srv_table(a.data());
    assert(changed.ptr!=repeat.ptr);check(changed,a.data(),srv_cpu_heap);assert(!memcmp(old.data(),(void*)first.ptr,128));
    // Reset with the SAME fence, including consecutive-key fast reuse.
    recycle();auto after=srv_table(a.data());assert(g.srv_ring_used==1);check(after,a.data(),srv_cpu_heap);
    // Depth/UAV helpers reserve entries in the same SRV ring without adding
    // them to this lookup. A cached base must not be derived from key count.
    g.srv_ring_used+=3;
    auto after_gap=srv_table(b.data());check(after_gap,b.data(),srv_cpu_heap);
    assert(after_gap.ptr==(uint64_t)(srv.bytes.data()+4*16));
    auto first_again=srv_table(a.data());check(first_again,a.data(),srv_cpu_heap);
    if(indexed)assert(first_again.ptr==after.ptr);
    for(unsigned i=0;i<128;i++){a.fill(0);a[0]=i;check(sampler_table(a.data()),a.data(),smp_cpu_heap);}
    assert(g.smp_ring_used==128);a[0]=0;assert(!sampler_table_needs_flush(a.data()));check(sampler_table(a.data()),a.data(),smp_cpu_heap);
    // A submit/fence advance does NOT rewind either descriptor ring. Sampler
    // tables must remain findable even at capacity, without extra copies.
    auto before_fence=sampler_table(a.data());auto before_copies=copies;
    g.fence_value++;assert(!sampler_table_needs_flush(a.data()));
    auto after_fence=sampler_table(a.data());assert(after_fence.ptr==before_fence.ptr&&copies==before_copies);
    check(after_fence,a.data(),smp_cpu_heap);
    a[0]=999;assert(sampler_table_needs_flush(a.data()));assert(!sampler_table(a.data()).ptr);
    recycle();
    a.fill(0);smp_cpu_heap.bytes[0]=0x99887766;
    check(sampler_table(a.data()),a.data(),smp_cpu_heap);assert(g.smp_ring_used==1);
    for(unsigned draw=0;draw<50000;draw++){
        if(draw%257==0){g.fence_value++;recycle();}
        if(draw%137==0){u32 slot=random32()%4096;*(u64*)srv_cpu_w(slot).ptr=random32();}
        for(unsigned i=0;i<16;i++){a[i]=((draw/3)%16)*16+i;b[i]=(draw%16)*16+i;}
        auto t=srv_table(a.data());assert(t.ptr);check(t,a.data(),srv_cpu_heap);
        check(sampler_table(b.data()),b.data(),smp_cpu_heap);
    }
    recycle();a.fill(0);
    for(u32 i=0;i<SRV_RING_TABLES;i++){a[0]=i%4096;a[1]=i/4096;assert(srv_table(a.data()).ptr);}
    assert(g.srv_ring_used==SRV_RING_TABLES);a[0]=999;a[1]=999;assert(!srv_table(a.data()).ptr);
    assert(srv.bytes.back()==guard&&smp.bytes.back()==guard);
    printf("descriptor tables %s: 50k draws, exact contents, source mutation, same-fence reset, fence without rewind, capacity, collisions/epoch wrap passed; copies=%llu bytes=%zu\n",argv[1],(unsigned long long)copies,sizeof(g_srv_table_entries)+sizeof(g_srv_table_buckets)+sizeof(g_smp_table_entries)+sizeof(g_smp_table_buckets));
}
