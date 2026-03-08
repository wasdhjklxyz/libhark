/**
 * @file export.h
 * @brief Symbol visibility macros for shared/static builds.
 */

#ifndef HARK_EXPORT_H
#define HARK_EXPORT_H

#if defined(_WIN32) || defined(__CYGWIN__)
#ifdef HARK_BUILDING
#define HARK_API __declspec(dllexport)
#else
#define HARK_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) && __GNUC__ >= 4
#define HARK_API __attribute__((visibility("default")))
#else
#define HARK_API
#endif

#ifdef HARK_STATIC
#undef HARK_API
#define HARK_API
#endif

#endif /* HARK_EXPORT_H */
