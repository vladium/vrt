#pragma once

#include "vr/config.h" // define _GNU_SOURCE
#include "vr/impl/macros_impl.h"

#include <boost/config.hpp>

//----------------------------------------------------------------------------

#if !defined (__GNUC__)
#   error expected a gcc-compatible compiler
#endif

// helper preprocessing macros:

#if !defined (VR_TO_STRING)
#   define VR_TO_STRING(x)          BOOST_PP_STRINGIZE (x)
#endif

#define VR_FILENAME                 ::vr::util::impl::path_stem (__FILE__)

// diagnostics:

#define VR_PRAGMA(x)                _Pragma ( VR_TO_STRING(x) )

#define VR_DIAGNOSTIC_PUSH()        VR_PRAGMA (GCC diagnostic push)
#define VR_DIAGNOSTIC_POP()         VR_PRAGMA (GCC diagnostic pop)

#define VR_DIAGNOSTIC_IGNORE(w)     VR_PRAGMA (GCC diagnostic ignored w)

// default input validation:

#if defined (NDEBUG)
#   define VR_DEBUG                 0
#   define VR_RELEASE               1
#   define VR_CHECK_INPUT           false
#else
#   define VR_DEBUG                 1
#   define VR_RELEASE               0
#   define VR_CHECK_INPUT           true
#endif

// trick macros (altering signatures depending on the build variant, etc):

#if defined (NDEBUG)
#   define VR_IF_DEBUG(...)
#   define VR_IF_RELEASE(...)       __VA_ARGS__
#else
#   define VR_IF_DEBUG(...)         __VA_ARGS__
#   define VR_IF_RELEASE(...)
#endif

//............................................................................

//  var/function qualifiers:

#define VR_FORCEINLINE              BOOST_FORCEINLINE
#define VR_NOINLINE                 BOOST_NOINLINE
#define VR_NOEXCEPT                 BOOST_NOEXCEPT_OR_NOTHROW
#define VR_NOEXCEPT_IF(condition)   BOOST_NOEXCEPT_IF(condition)
#define VR_NORETURN                 BOOST_NORETURN

#define VR_SO_PUBLIC                __attribute__ ((visibility ("default")))
#define VR_SO_LOCAL                 __attribute__ ((visibility ("hidden")))

#define VR_ASSUME_HOT               __attribute__ ((hot))
#define VR_ASSUME_COLD              __attribute__ ((cold))

#if defined (NDEBUG)
#   define VR_PURE                  __attribute__ ((const)) // note: intentionally not using '__attribute__ ((pure))'
#else
#   define VR_PURE                  // undefined because VR_ASSUME_UNREACHABLE() would violate purity in debug builds
#endif

#define VR_UNUSED                   __attribute__ ((unused))

//  alignment/aliasing:

#define VR_RESTRICT                 __restrict__

#define VR_PACKED                   __attribute__ ((packed))

#define VR_ASSUME_ALIGNED(exp, value)   __builtin_assume_aligned ((exp), value)

//  branches:

#define VR_LIKELY(condition)        __builtin_expect(static_cast<bool> (condition), 1)
#define VR_UNLIKELY(condition)      __builtin_expect(static_cast<bool> (condition), 0)

#define VR_LIKELY_VALUE(exp, value) __builtin_expect(static_cast<long> (exp), (value))

#if defined (NDEBUG)
#   define VR_ASSUME_UNREACHABLE(...)   __builtin_unreachable()
#else
#   define VR_ASSUME_UNREACHABLE(...)   throw_x (illegal_state, "reached code assumed unreachable" vr_ASSUME_UNREACHABLE_expand_args (__VA_ARGS__))
#endif

#define VR_UNGUARDED_FIELD_ERROR     "unguarded elided field access"
#define VR_UNGUARDED_METHOD_ERROR    "unguarded elided method invocation"

//----------------------------------------------------------------------------
