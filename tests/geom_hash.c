#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
typedef uint8_t u8;
typedef uint64_t u64;
#include "../third_party/ps3recomp/libs/video/rsx_geom_hash.h"
int main(void)
{
    /* All byte positions, including unaligned starts, SIMD tails and the
     * short/long transition, must affect the complete two-word key. */
    static u8 data[8192 + 32];
    const size_t sizes[] = {0,1,7,8,31,63,64,65,127,128,129,240,241,1024,8192};
    for (unsigned k = 0; k < sizeof sizes / sizeof sizes[0]; k++) {
        const size_t n = sizes[k];
        for (unsigned offset = 0; offset < 16; offset++) {
            for (size_t i = 0; i < n; i++) data[offset+i] = (u8)(i * 73 + 19);
            u64 a = 1, b = 2, a2 = 1, b2 = 2;
            gk_bytes_update(&a, &b, data + offset, n, 1);
            gk_bytes_update(&a2, &b2, data + offset, n, 1);
            assert(a == a2 && b == b2);
            if (n < 64) {
                a2=1; b2=2; gk_bytes_legacy(&a2,&b2,data+offset,n);
                assert(a==a2 && b==b2);
            }
            for (size_t i = 0; i < n; i++) {
                data[offset+i] ^= 1;
                a2=1; b2=2; gk_bytes_update(&a2,&b2,data+offset,n,1);
                assert(a != a2 || b != b2);
                data[offset+i] ^= 1;
            }
            a2=1; b2=3; gk_bytes_update(&a2,&b2,data+offset,n,1);
            assert(a != a2 || b != b2); /* preserve both incoming key components */
        }
    }
    puts("geometry hash: all-byte coverage, bounds, tails and both key components passed");
}
