#include "util.hpp"

size_t copy_c_str(char * dst, const char * src, size_t buffer_size)
{
    if (dst == NULL || src == NULL || buffer_size == 0)
        return 0;

    size_t i;
    for (i = 0; i < buffer_size - 1 && src[i] != '\0'; i++)
        dst[i] = src[i];

    dst[i] = '\0';
    return i;
}
