#pragma once

#include "vr/exceptions.h"
#include "vr/meta/integer.h" // static_range
#include "vr/preprocessor.h" // VR_VARIADIC_TO_LIST
#include "vr/strings_fwd.h"
#include "vr/str_hash.h"
#include "vr/util/traits.h"  // generic_trait_set

#include <boost/preprocessor/cat.hpp>
#include <boost/preprocessor/control/if.hpp>
#include <boost/preprocessor/punctuation/comma.hpp>
#include <boost/preprocessor/punctuation/comma_if.hpp>
#include <boost/preprocessor/seq/seq.hpp>
#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/seq/for_each_i.hpp>
#include <boost/preprocessor/tuple/to_seq.hpp>
#include <boost/preprocessor/variadic/to_seq.hpp>

#include <cstring> // std::strlen()
#include <tuple>

//----------------------------------------------------------------------------
namespace vr // define some enum-specific trait tags:
{
    struct _iterable_       { };
    struct _range_          { };
    struct _printable_      { };
    struct _parsable_       { };

} // end of namespace
//----------------------------------------------------------------------------

#define vr_ENUM_PROLOGUE(name) \
struct name \
{ \
/* */

#define vr_ENUM_EPILOGUE() \
} /* user provides final semicolon */ \
/* */

//............................................................................
//............................................................................
// VR[_NESTED]_ENUM impl:
//............................................................................
/*
 * this is slightly tricky: because the same type of *_FOR_EACH macros are not reentrant,
 * use a list to iterate over options instead of a seq at this nesting level
 */
#define vr_ENUM_OPTIONS(val_seq, .../* opts */) \
    BOOST_PP_LIST_FOR_EACH (vr_ENUM_emit_opt, val_seq, VR_VARIADIC_TO_LIST (__VA_ARGS__)) \
    /* */

#define vr_ENUM_emit_opt(r, val_seq, opt) \
    BOOST_PP_CAT (vr_ENUM_opt_, opt)(val_seq) \
    /* */

//............................................................................

#define vr_ENUM_TRAITS(.../* opts */) \
    using enum_traits = generic_trait_set \
        <  \
            BOOST_PP_LIST_FOR_EACH_I (vr_ENUM_emit_trait, unused, VR_VARIADIC_TO_LIST (__VA_ARGS__)) \
        >; \
    /* */

#define vr_ENUM_emit_trait(r, data, i, opt) \
            BOOST_PP_COMMA_IF (i) BOOST_PP_CAT (_, BOOST_PP_CAT (opt, _)) \
    /* */

//............................................................................

#define VR_ENUM_NA_NAME     "NA"

//............................................................................

#define vr_ENUM_VALUES_value(r, data, i, val) \
    val BOOST_PP_COMMA () \
    BOOST_PP_IF (i, BOOST_PP_EMPTY, first = data BOOST_PP_COMMA)() \
    /* */

#define vr_ENUM_VALUES_seq(val_seq) \
    BOOST_PP_SEQ_FOR_EACH_I (vr_ENUM_VALUES_value, BOOST_PP_SEQ_HEAD (val_seq), val_seq) \
    /* */

#define vr_ENUM_VALUES(val_seq) \
    enum enum_t : enum_int_t \
    { \
        vr_ENUM_VALUES_seq (val_seq) \
        size, \
        na = enum_NA () \
    }; \
    /* */

//............................................................................
// 'iterable':
//............................................................................

#define vr_ENUM_opt_iterable(val_seq) \
    friend enum_t operator++ (enum_t & e) VR_NOEXCEPT \
    { \
        return (e = static_cast<enum_t> (e + 1)); \
    } \
    \
    friend enum_t begin (enum_t const & e) VR_NOEXCEPT \
    { \
        return e; \
    } \
    friend enum_t end (enum_t const &) VR_NOEXCEPT \
    { \
        return size; \
    } \
    \
    friend enum_t operator* (enum_t const & e) VR_NOEXCEPT \
    { \
        return e; \
    } \
    \
    static enum_t values () VR_NOEXCEPT \
    { \
        return first; \
    } \
    /* */

//............................................................................
// 'printable':
//............................................................................

#define vr_ENUM_opt_printable(val_seq) \
    static VR_PURE string_literal_t name (enum_t const e) VR_NOEXCEPT \
    { \
        switch (e) \
        { \
            BOOST_PP_SEQ_FOR_EACH (vr_ENUM_opt_printable_case, unused, val_seq) \
          \
            case na: return VR_ENUM_NA_NAME; \
            default: return "??"; \
        } \
    } \
    \
    friend VR_PURE std::string __print__ (enum_t const & e) VR_NOEXCEPT \
    { \
        switch (e) \
        { \
            BOOST_PP_SEQ_FOR_EACH (vr_ENUM_opt_printable_print_case, unused, val_seq) \
          \
            case na: return VR_ENUM_NA_NAME; /* NOTE: unquoted */ \
            default: return "??"; \
        } \
    } \
    \
    friend std::ostream & operator<< (std::ostream & os, enum_t const & e) VR_NOEXCEPT \
    { \
        return os << name (e); \
    } \
    /* */

#   define vr_ENUM_opt_printable_case(r, unused, val) \
        case val : return VR_TO_STRING (val) ; \
        /* */

#   define vr_ENUM_opt_printable_print_case(r, unused, val) \
        case val : return "\"" VR_TO_STRING (val) "\""; /* quoted */ \
        /* */

//............................................................................
// 'parsable':
//............................................................................

#define vr_ENUM_opt_parsable(val_seq) \
    static enum_t value (char_const_ptr_t const name, int32_t const name_size) \
    { \
        switch (str_hash_32 (name, name_size)) \
        { \
            BOOST_PP_SEQ_FOR_EACH (vr_ENUM_opt_parsable_case, unused, val_seq) \
            case "NA"_hash : return na ; \
        } \
        \
        throw_x (invalid_input, "invalid enum value name '" + std::string (name, name_size) + '\''); \
    } \
    \
    static VR_FORCEINLINE enum_t value (string_literal_t const name) \
    { \
        return value (name, std::strlen (name)); \
    } \
    \
    static VR_FORCEINLINE enum_t value (std::string const & name) \
    { \
        return value (name.data (), name.size ()); \
    } \
    /* */

#   define vr_ENUM_opt_parsable_case(r, unused, val) \
        case BOOST_PP_CAT (VR_TO_STRING (val), _hash) : return val ; \
        /* */

//............................................................................
//............................................................................
// VR[_NESTED]_ENUM impl:
//............................................................................
/*
 * value -> BOOST_PP_TUPLE_ELEM (2, 0, val_name_tup)
 * name  -> BOOST_PP_TUPLE_ELEM (2, 1, val_name_tup)
 */
#define vr_EXPLICIT_ENUM_VALUES_value(r, data, i, val_name_tup) \
    BOOST_PP_COMMA_IF (i) \
    BOOST_PP_TUPLE_ELEM (2, 1, val_name_tup) = BOOST_PP_TUPLE_ELEM (2, 0, val_name_tup) \
    /* */

#define vr_EXPLICIT_ENUM_VALUES(type, val_name_seq) \
    enum enum_t : type \
    { \
        BOOST_PP_SEQ_FOR_EACH_I (vr_EXPLICIT_ENUM_VALUES_value, unused, val_name_seq) \
    }; \
    /* */
//............................................................................
/*
 * this is slightly tricky: because the same type of *_FOR_EACH macros are not reentrant,
 * use a list to iterate over options instead of a seq at this nesting level
 */
#define vr_EXPLICIT_ENUM_OPTIONS(val_name_seq, ...) \
    BOOST_PP_LIST_FOR_EACH (vr_EXPLICIT_ENUM_emit_opt, val_name_seq, VR_VARIADIC_TO_LIST (__VA_ARGS__)) \
    /* */

#define vr_EXPLICIT_ENUM_emit_opt(r, val_name_seq, opt) \
    BOOST_PP_CAT (vr_EXPLICIT_ENUM_opt_, opt)(val_name_seq) \
    /* */

//............................................................................
// 'range':
//............................................................................

#define vr_EXPLICIT_ENUM_opt_range(val_name_seq) \
        static constexpr std::tuple<enum_t, enum_t> enum_range () \
            { \
                using r     = meta::static_range<enum_t BOOST_PP_SEQ_FOR_EACH (vr_EXPLICIT_ENUM_opt_range_e, unused, val_name_seq)>; \
                \
                return std::make_tuple (r::min (), r::max ()); \
            } \
        /* */

#   define vr_EXPLICIT_ENUM_opt_range_e(r, unused, val_name_tup) \
        , BOOST_PP_TUPLE_ELEM (2, 1, val_name_tup) \
        /* */
//............................................................................
// 'printable':
//............................................................................

#define vr_EXPLICIT_ENUM_opt_printable(val_name_seq) \
    static VR_PURE string_literal_t name (enum_t const e) VR_NOEXCEPT \
    { \
        switch (e) \
        { \
            BOOST_PP_SEQ_FOR_EACH (vr_EXPLICIT_ENUM_opt_printable_case, unused, val_name_seq) \
          \
            default: return "??"; \
        } \
    } \
    \
    friend VR_PURE std::string __print__ (enum_t const & e) VR_NOEXCEPT \
    { \
        switch (e) \
        { \
            BOOST_PP_SEQ_FOR_EACH (vr_EXPLICIT_ENUM_opt_printable_print_case, unused, val_name_seq) \
          \
            default: return "??"; \
        } \
    } \
    \
    friend std::ostream & operator<< (std::ostream & os, enum_t const & e) VR_NOEXCEPT \
    { \
        return os << name (e); \
    } \
    /* */

#   define vr_EXPLICIT_ENUM_opt_printable_case(r, unused, val_name_tup) \
        case BOOST_PP_TUPLE_ELEM (2, 0, val_name_tup) : return VR_TO_STRING (BOOST_PP_TUPLE_ELEM (2, 1, val_name_tup)) ; \
        /* */

#   define vr_EXPLICIT_ENUM_opt_printable_print_case(r, unused, val_name_tup) \
        case BOOST_PP_TUPLE_ELEM (2, 0, val_name_tup) : return "\"" VR_TO_STRING (BOOST_PP_TUPLE_ELEM (2, 1, val_name_tup)) "\""; /* quoted */ \
        /* */

//............................................................................
// 'parsable':
//............................................................................

#define vr_EXPLICIT_ENUM_opt_parsable(val_name_seq) \
    static enum_t value (char_const_ptr_t const name, int32_t const name_size) \
    { \
        switch (str_hash_32 (name, name_size)) \
        { \
            BOOST_PP_SEQ_FOR_EACH (vr_EXPLICIT_ENUM_opt_parsable_case, unused, val_name_seq) \
        } \
        \
        throw_x (invalid_input, "invalid enum value name '" + std::string (name, name_size) + '\''); \
    } \
    \
    static VR_FORCEINLINE enum_t value (string_literal_t const name) \
    { \
        return value (name, std::strlen (name)); \
    } \
    \
    static VR_FORCEINLINE enum_t value (std::string const & name) \
    { \
        return value (name.data (), name.size ()); \
    } \
    /* */

#   define vr_EXPLICIT_ENUM_opt_parsable_case(r, unused, val_name_tup) \
        case BOOST_PP_CAT (VR_TO_STRING (BOOST_PP_TUPLE_ELEM (2, 1, val_name_tup)), _hash) : return static_cast<enum_t> (BOOST_PP_TUPLE_ELEM (2, 0, val_name_tup)); \
        /* */

//----------------------------------------------------------------------------
