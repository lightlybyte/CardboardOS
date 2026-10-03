#ifndef NOTC_H
#define NOTC_H

#include <stdint.h>
#include <stddef.h>

#define NOTC_OP_HALT  0x00
#define NOTC_OP_PUSH  0x01
#define NOTC_OP_WRITE 0x02

#define NOTC_MAX_CODE       4096
#define NOTC_MAX_STRINGS    256
#define NOTC_MAX_STRING_LEN 256

struct notc_program {
    uint32_t code[NOTC_MAX_CODE];
    uint32_t code_len;
    char     strings[NOTC_MAX_STRINGS][NOTC_MAX_STRING_LEN];
    uint32_t string_count;
};

int notc_load(const uint8_t *data, size_t len, struct notc_program *out);
int notc_run(const struct notc_program *prog);
int notc_compile(const char *source, uint8_t *out, size_t outsz, size_t *outlen);

#endif