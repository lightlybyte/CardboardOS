#include "notc.h"
#include <string.h>
#include <stdint.h>

extern void notc_write_hook(const char *s);

static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

int notc_load(const uint8_t *data, size_t len, struct notc_program *out) {
    if (len < 16) return -1;
    if (data[0] != 'C' || data[1] != 'A' || data[2] != '0' || data[3] != '1')
        return -1;

    uint32_t code_len   = rd32(data + 4);
    uint32_t strtab_len = rd32(data + 8);
    uint32_t strtab_off = rd32(data + 12);

    if (code_len > NOTC_MAX_CODE) return -1;
    if ((size_t)strtab_off + strtab_len > len) return -1;

    out->code_len = code_len;
    for (uint32_t i = 0; i < code_len; i++)
        out->code[i] = rd32(data + 16 + i * 4);

    uint32_t p = strtab_off;
    uint32_t end = strtab_off + strtab_len;
    out->string_count = 0;
    while (p + 2 <= end && out->string_count < NOTC_MAX_STRINGS) {
        uint32_t slen = (uint32_t)data[p] | ((uint32_t)data[p+1] << 8);
        p += 2;
        if (p + slen > end) break;
        if (slen >= NOTC_MAX_STRING_LEN) slen = NOTC_MAX_STRING_LEN - 1;
        memcpy(out->strings[out->string_count], data + p, slen);
        out->strings[out->string_count][slen] = 0;
        out->string_count++;
        p += slen;
    }
    return 0;
}

int notc_run(const struct notc_program *prog) {
    uint32_t stack[64];
    int sp = 0;

    for (uint32_t pc = 0; pc < prog->code_len; pc++) {
        uint32_t ins = prog->code[pc];
        uint8_t op = (uint8_t)(ins & 0xFF);
        uint32_t arg = ins >> 8;

        switch (op) {
            case NOTC_OP_HALT:
                return 0;
            case NOTC_OP_PUSH:
                if (sp >= 64) return -1;
                stack[sp++] = arg;
                break;
            case NOTC_OP_WRITE: {
                if (sp <= 0) return -1;
                uint32_t idx = stack[--sp];
                if (idx >= prog->string_count) return -1;
                notc_write_hook(prog->strings[idx]);
                break;
            }
            default:
                return -1;
        }
    }
    return 0;
}

static void emit32(uint8_t *out, size_t *pos, uint32_t v) {
    out[(*pos)++] = (uint8_t)(v & 0xFF);
    out[(*pos)++] = (uint8_t)((v >> 8) & 0xFF);
    out[(*pos)++] = (uint8_t)((v >> 16) & 0xFF);
    out[(*pos)++] = (uint8_t)((v >> 24) & 0xFF);
}

int notc_compile(const char *source, uint8_t *out, size_t outsz, size_t *outlen) {
    uint32_t code[NOTC_MAX_CODE];
    uint32_t code_len = 0;

    char strings[NOTC_MAX_STRINGS][NOTC_MAX_STRING_LEN];
    uint32_t string_count = 0;
    uint32_t string_lengths[NOTC_MAX_STRINGS];

    const char *p = source;

#define SKIP_WS() while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++

    SKIP_WS();
    if (strncmp(p, "main", 4) != 0) return -1;
    p += 4;
    SKIP_WS();
    if (*p != ':') return -1;
    p++;
    SKIP_WS();
    if (*p != '[') return -1;
    p++;

    for (;;) {
        SKIP_WS();
        if (*p == 0) return -1;
        if (*p == ']') { p++; break; }

        if (strncmp(p, "write", 5) != 0) return -1;
        p += 5;
        SKIP_WS();
        if (*p != '(') return -1;
        p++;
        SKIP_WS();
        if (*p != '"') return -1;
        p++;

        if (string_count >= NOTC_MAX_STRINGS) return -1;
        char *dst = strings[string_count];
        uint32_t n = 0;
        while (*p && *p != '"') {
            if (*p == '\\' && *(p+1)) {
                p++;
                if      (*p == 'n') *dst++ = '\n';
                else if (*p == 't') *dst++ = '\t';
                else if (*p == '\\') *dst++ = '\\';
                else                *dst++ = *p;
                p++;
            } else {
                *dst++ = *p++;
            }
            if (++n >= NOTC_MAX_STRING_LEN - 1) return -1;
        }
        if (*p != '"') return -1;
        p++;
        *dst = 0;
        string_lengths[string_count] = n;
        string_count++;

        SKIP_WS();
        if (*p != ')') return -1;
        p++;

        if (code_len + 2 > NOTC_MAX_CODE) return -1;
        code[code_len++] = (NOTC_OP_PUSH) | ((string_count - 1) << 8);
        code[code_len++] = (NOTC_OP_WRITE);
    }

    if (code_len + 1 > NOTC_MAX_CODE) return -1;
    code[code_len++] = (NOTC_OP_HALT);

    size_t header = 16;
    size_t code_bytes = code_len * 4;
    size_t strtab_bytes = 0;
    for (uint32_t i = 0; i < string_count; i++)
        strtab_bytes += 2 + string_lengths[i];

    size_t total = header + code_bytes + strtab_bytes;
    if (total > outsz) return -1;

    out[0] = 'C'; out[1] = 'A'; out[2] = '0'; out[3] = '1';

    size_t pos = 4;
    emit32(out, &pos, code_len);
    emit32(out, &pos, (uint32_t)strtab_bytes);
    emit32(out, &pos, (uint32_t)(header + code_bytes));

    for (uint32_t i = 0; i < code_len; i++)
        emit32(out, &pos, code[i]);

    for (uint32_t i = 0; i < string_count; i++) {
        out[pos++] = (uint8_t)(string_lengths[i] & 0xFF);
        out[pos++] = (uint8_t)((string_lengths[i] >> 8) & 0xFF);
        memcpy(out + pos, strings[i], string_lengths[i]);
        pos += string_lengths[i];
    }

    *outlen = pos;
    return 0;
}