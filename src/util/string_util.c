#include "util/string_util.h"

#include <stdlib.h>
#include <string.h>

char *duplicate_string(const char *source)
{
    if (source == NULL)
    {
        return NULL;
    }

    const size_t source_length = strlen(source);
    char *copy = malloc(source_length + 1U);
    if (copy == NULL)
    {
        return NULL;
    }

    memcpy(copy, source, source_length + 1U);
    return copy;
}
