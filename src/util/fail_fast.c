#include "util/fail_fast.h"

#include <SDL3/SDL.h>

#include <stdarg.h>
#include <stdlib.h>

_Noreturn void fail_fast(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_CRITICAL, format, args);
    va_end(args);
    abort();
}
