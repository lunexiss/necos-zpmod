#pragma once
#ifndef SAFE_SNPRINTF_H
#define SAFE_SNPRINTF_H

#include "build.h"

#if defined(_MSC_VER) || defined(XASH_WIN32)
#ifdef __cplusplus
extern "C" {
#endif
int safe_snprintf(char *buffer, int buffersize, const char *format, ...);
#ifdef __cplusplus
}
#endif
#else
#include <stdio.h>
#define safe_snprintf snprintf
#define safe_vsnprintf vsnprintf
#endif

#endif // SAFE_SNPRINTF_H
