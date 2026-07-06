#pragma once

/*
 * VS Code's C/C++ IntelliSense can inherit host macOS macros while parsing
 * devkitPro Switch headers. The real devkitA64 build does not define these.
 */
#ifdef __APPLE__
#undef __APPLE__
#endif

#ifdef __MACH__
#undef __MACH__
#endif

#ifdef __linux__
#undef __linux__
#endif

#ifdef __linux
#undef __linux
#endif

#ifdef linux
#undef linux
#endif

#ifndef __SWITCH__
#define __SWITCH__ 1
#endif

#ifndef __ORDER_LITTLE_ENDIAN__
#define __ORDER_LITTLE_ENDIAN__ 1234
#endif

#ifndef __ORDER_BIG_ENDIAN__
#define __ORDER_BIG_ENDIAN__ 4321
#endif

#ifndef __BYTE_ORDER__
#define __BYTE_ORDER__ __ORDER_LITTLE_ENDIAN__
#endif

#ifndef __FLOAT_WORD_ORDER__
#define __FLOAT_WORD_ORDER__ __ORDER_LITTLE_ENDIAN__
#endif
