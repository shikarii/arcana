#include "platform.h"
#include <time.h>

/* --- String duplication --- */
char* arc_platform_strdup(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char* dup = (char*)malloc(len);
    if (dup) memcpy(dup, s, len);
    return dup;
}

/* --- File I/O --- */
uint8_t* arc_platform_read_file(const char* path, size_t* out_len) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return NULL; }
    fseek(f, 0, SEEK_SET);

    uint8_t* buf = (uint8_t*)malloc((size_t)sz);
    if (!buf) { fclose(f); return NULL; }

    size_t rd = fread(buf, 1, (size_t)sz, f);
    fclose(f);

    if (rd != (size_t)sz) { free(buf); return NULL; }
    if (out_len) *out_len = rd;
    return buf;
}

ArcStatus arc_platform_write_file(const char* path, const uint8_t* data, size_t len) {
    FILE* f = fopen(path, "wb");
    if (!f) return ARC_ERR_IO;

    size_t wr = fwrite(data, 1, len, f);
    fclose(f);
    return (wr == len) ? ARC_OK : ARC_ERR_IO;
}

/* --- Monotonic clock --- */
double arc_platform_clock(void) {
    return (double)clock() / (double)CLOCKS_PER_SEC;
}

/* --- Terminal detection --- */
#ifdef _WIN32
#include <io.h>
bool arc_platform_is_terminal(FILE* f) {
    return _isatty(_fileno(f)) != 0;
}
#else
#include <unistd.h>
bool arc_platform_is_terminal(FILE* f) {
    return isatty(fileno(f)) != 0;
}
#endif
