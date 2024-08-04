#include "stdint.h"
#include "stdio.h"
#include "stdlib.h"
#include "unistd.h"

/*** Unicode and UTF-8 ***/
void writeu8(int __fd, uint32_t codepoint) {
    uint32_t c = codepoint;
    if (codepoint > 0x7F) {
        c = (codepoint & 0x000003F) | (codepoint & 0x0000FC0) << 2 | (codepoint & 0x003F000) << 4 | (codepoint & 0x01C0000) << 6;
        if (codepoint < 0x0000800)
            c |= 0x0000C080;
        else if (codepoint < 0x0010000)
            c |= 0x00E08080;
        else
            c |= 0xF0808080;
    }
    uint8_t b1 = c & 0x000000FF;
    uint8_t b2 = ((c & 0x0000FF00) >> 8);
    uint8_t b3 = ((c & 0x00FF0000) >> 16);
    uint8_t b4 = ((c & 0xFF000000) >> 24);
    if (b4)
        write(__fd, &b4, 1);
    if (b3)
        write(__fd, &b3, 1);
    if (b2)
        write(__fd, &b2, 1);
    if (b1)
        write(__fd, &b1, 1);
}

int main() {
    srand(0);
    // for (int i = 0; i < 10; i++)
    //     printf("%c\n", rand() % 26 + 'A');
    // u8chr_t c1 = u8encode(0x0370);
    // printf("%x\n", c1);
    for (uint32_t i = 0x2500; i <= 0x2590; i += 0x000F) {
        for (uint32_t j = 0; j < 16; j++) {
            writeu8(STDOUT_FILENO, i + j);
        }
        printf("\n");
    }
    printf("\n");
    for (uint32_t i = 0x1FB00; i <= 0x1FBF0; i += 0x000F) {
        for (uint32_t j = 0; j < 16; j++) {
            writeu8(STDOUT_FILENO, i + j);
        }
        printf("\n");
    }
    printf("\n");
    for (uint32_t i = 0x30A0; i <= 0x30F0; i += 0x0010) {
        for (uint32_t j = 0; j < 16; j++) {
            writeu8(STDOUT_FILENO, i + j);
        }
        printf("\n");
    }
    printf("\n");
    // for (uint32_t i = 0x0; i <= 0xFFF0; i += 0x0010) {
    //     for (uint32_t j = 0; j < 16; j++) {
    //         writeu8(STDOUT_FILENO, i + j);
    //     }
    //     printf("\n");
    // }
    // writeu8(STDOUT_FILENO, 0x1FB00);
}