/* Compare the actual batch decoder against its scalar path, including every
 * half bit pattern, float NaNs, 20-bit index wrap and frequency operations. */
#include "../third_party/ps3recomp/libs/video/rsx_vertex_compact.c"
#include <stdio.h>
#include <assert.h>
static unsigned char data[16 << 20];
/* Plans are constructed directly below; register decoding is not under test. */
u32 rsx_dsp_vertex_data_base_offset(const rsx_dispatch* rsx) { (void)rsx; abort(); }
void rsx_dsp_get_vertex_attr(const rsx_dispatch* rsx, u32 index, rsx_dsp_vertex_attr* out)
{ (void)rsx; (void)index; (void)out; abort(); }
void rsx_dsp_vertex_default(const rsx_dispatch* rsx, u32 index, float out[4])
{ (void)rsx; (void)index; (void)out; abort(); }
static const u8* memory(void* user, u32 location, u32 off, u32 bytes)
{ (void)user; (void)location; return off <= sizeof data && bytes <= sizeof data - off ? data + off : NULL; }
static void check(u32 type, u32 size, u32 frequency, u32 modulo)
{
    rsx_vertex_fetch_plan p = {0};
    rsx_vertex_layout_plan_init(&p.layout, 1);
    p.guest_ptr = memory; p.divider_mask = modulo;
    p.attr[0].desc.type = type; p.attr[0].desc.size = size;
    p.attr[0].desc.frequency = frequency;
    p.attr[0].elem_size = p.attr[0].stride = rsx_vertex_attrib_size(type, size);
    rsx_vertex_ref refs[256];
    u8 batch[256 * 16], scalar[16];
    for (u32 offset = 0; offset < 65536; offset += 256) {
        for (u32 i = 0; i < 256; i++) { refs[i].vertex_id = offset + i; refs[i].base_index = 0; }
        rsx_vertex_fetch_plan_prepare(&p, refs, 256);
        assert(rsx_vertex_fetch_batch(&p, refs, 256, batch, 16));
        for (u32 i = 0; i < 256; i++) {
            assert(rsx_vertex_fetch_one(&p, &refs[i], scalar));
            assert(memcmp(batch + i * 16, scalar, 16) == 0);
        }
    }
}
int main(void)
{
    for (u32 i = 0; i < 65536; i++) {
        /* Each of the four half lanes sees all 65536 encodings. */
        for (u32 lane = 0; lane < 4; lane++) {
            data[i * 8 + lane * 2] = (u8)(i >> 8);
            data[i * 8 + lane * 2 + 1] = (u8)i;
        }
    }
    for (u32 size = 1; size <= 4; size++) check(RSX_VTX_TYPE_HALF, size, 1, 0);
    unsigned r = 7;
    for (u32 i = 0; i < sizeof data; i++) { r = r * 1664525u + 1013904223u; data[i] = (u8)(r >> 24); }
    for (u32 type = 1; type <= 7; type++)
        for (u32 size = 1; size <= 4; size++) {
            check(type, size, 1, 0);
            check(type, size, 7, 0);
            check(type, size, 7, 1);
        }
    rsx_vertex_fetch_plan p = {0};
    rsx_vertex_layout_plan_init(&p.layout, 7);
    p.guest_ptr = memory;
    for (u32 a = 0; a < 3; a++) {
        p.attr[a].desc.type = RSX_VTX_TYPE_FLOAT; p.attr[a].desc.size = 4;
        p.attr[a].elem_size = p.attr[a].stride = 16;
    }
    p.attr[1].desc.frequency = 3; /* divide, ignores base index */
    p.attr[2].desc.frequency = 7; p.divider_mask = 4; /* modulo */
    const rsx_vertex_ref refs[] = {{0xfffff, 1}, {0xffffe, 2}, {3, 0}};
    rsx_vertex_fetch_plan_prepare(&p, refs, 3);
    assert(p.attr[0].stable_first == 0 && p.attr[0].stable_last == 3);
    assert(p.attr[1].stable_first == 1 && p.attr[1].stable_last == 0xfffff / 3);
    assert(p.attr[2].stable_first == 0xffffe % 7 && p.attr[2].stable_last == 3);
    puts("vertex batch: scalar equivalence and wrapped/instanced bounds passed");
}
