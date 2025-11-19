#pragma once

#if defined(__clang__) || defined(__GNUC__)
#define LIKELY(expr)   (__builtin_expect(!!(expr), 1))
#define UNLIKELY(expr) (__builtin_expect(!!(expr), 0))
#else
#define LIKELY(expr)   (expr)
#define UNLIKELY(expr) (expr)
#endif
