#pragma once

#include "vr/exceptions.h"
#include "vr/impl/asserts_impl.h"
#include "vr/strings.h"
#include "vr/util/conditions.h"

//----------------------------------------------------------------------------

#define vr_static_assert(...) static_assert (__VA_ARGS__, #__VA_ARGS__)

//............................................................................
// NOTE: all of these throw 'invalid_input' (or derivatives) on failure:
//............................................................................

// evaluated in both release and debug builds:

#define check_condition(condition, /* extra context */...) \
    do { if (VR_UNLIKELY (! (condition))) \
        throw_x (::vr::check_failure, "failed: (" VR_TO_STRING (condition) ")" \
            vr_CHECK_expand_context (__VA_ARGS__) ); } while (false) \
    /* */

/**
 * i in [0, limit)
 */
#define check_within(i, limit, /* extra context */...) \
    do { if (VR_UNLIKELY (! (vr_is_within (i, limit)))) \
        throw_x (::vr::out_of_bounds, "failed: expected " VR_TO_STRING (i) " (" + ::vr::print (i) + ") in [0, " + ::vr::print (limit) + ")" \
            vr_CHECK_expand_context (__VA_ARGS__) ); } while (false) \
    /* */

/**
 * i in [0, limit]
 */
#define check_within_inclusive(i, limit, /* extra context */...) \
    do { if (VR_UNLIKELY (! (vr_is_within_inclusive (i, limit)))) \
        throw_x (::vr::out_of_bounds, "failed: expected " VR_TO_STRING (i) " (" + ::vr::print (i) + ") in [0, " + ::vr::print (limit) + "]" \
            vr_CHECK_expand_context (__VA_ARGS__) ); } while (false) \
    /* */

/**
 * i in [a, b)
 */
#define check_in_range(i, a, b, /* extra context */...) \
    do { if (VR_UNLIKELY (! (vr_is_in_range (i, a, b)))) \
        throw_x (::vr::out_of_bounds, "failed: expected " VR_TO_STRING (i) " (" + ::vr::print (i) + ") in [" + ::vr::print (a) + ", " + ::vr::print (b) + ")" \
            vr_CHECK_expand_context (__VA_ARGS__) ); } while (false) \
    /* */

/**
 * i in [a, b]
 */
#define check_in_inclusive_range(i, a, b, /* extra context */...) \
    do { if (VR_UNLIKELY (! (vr_is_in_inclusive_range (i, a, b)))) \
        throw_x (::vr::out_of_bounds, "failed: expected " VR_TO_STRING (i) " (" + ::vr::print (i) + ") in [" + ::vr::print (a) + ", " + ::vr::print (b) + "]" \
            vr_CHECK_expand_context (__VA_ARGS__) ); } while (false) \
    /* */

/**
 * i in (a, b)
 */
#define check_in_exclusive_range(i, a, b, /* extra context */...) \
    do { if (VR_UNLIKELY (! (vr_is_in_exclusive_range (i, a, b)))) \
        throw_x (::vr::out_of_bounds, "failed: expected " VR_TO_STRING (i) " (" + ::vr::print (i) + ") in (" + ::vr::print (a) + ", " + ::vr::print (b) + ")" \
            vr_CHECK_expand_context (__VA_ARGS__) ); } while (false) \
    /* */

#define check_is_power_of_2(i, /* extra context */...) \
    do { if (VR_UNLIKELY (! (vr_is_power_of_2 (i)))) \
        throw_x (::vr::invalid_input, "failed: expected " VR_TO_STRING (i) " (" + ::vr::print (i) + ") to be a power of 2" \
            vr_CHECK_expand_context (__VA_ARGS__) ); } while (false) \
    /* */


#define check_positive(v, /* extra context */...) \
    do { if (VR_UNLIKELY (! (vr_is_positive (v)))) \
        throw_x (::vr::invalid_input, "failed: expected positive " VR_TO_STRING (v) " (" + ::vr::print (v) + ")" \
            vr_CHECK_expand_context (__VA_ARGS__) ); } while (false) \
    /* */

#define check_nonnegative(v, /* extra context */...) \
    do { if (VR_UNLIKELY (! (vr_is_nonnegative (v)))) \
        throw_x (::vr::invalid_input, "failed: expected nonnegative " VR_TO_STRING (v) " (" + ::vr::print (v) + ")" \
            vr_CHECK_expand_context (__VA_ARGS__) ); } while (false) \
    /* */

#define check_zero(v, /* extra context */...) \
    do { if (VR_UNLIKELY (! (vr_is_zero (v)))) \
        throw_x (::vr::invalid_input, "failed: expected zero " VR_TO_STRING (v) " (" + ::vr::print (v) + ")" \
            vr_CHECK_expand_context (__VA_ARGS__) ); } while (false) \
    /* */

#define check_nonzero(v, /* extra context */...) \
    do { if (VR_UNLIKELY (! (vr_is_nonzero (v)))) \
        throw_x (::vr::invalid_input, "failed: expected nonzero " VR_TO_STRING (v) \
            vr_CHECK_expand_context (__VA_ARGS__) ); } while (false) \
    /* */



#define check_eq(lhs, rhs, /* extra context */...) \
    do { if (VR_UNLIKELY (! (vr_eq (lhs, rhs)))) \
        throw_x (::vr::invalid_input, "failed: expected " VR_TO_STRING (lhs) " (" + ::vr::print (lhs) + ") == " VR_TO_STRING (rhs) + " (" + ::vr::print (rhs) + ")" \
            vr_CHECK_expand_context (__VA_ARGS__) ); } while (false) \
    /* */

#define check_ne(lhs, rhs, /* extra context */...) \
    do { if (VR_UNLIKELY (! (vr_ne (lhs, rhs)))) \
        throw_x (::vr::invalid_input, "failed: expected " VR_TO_STRING (lhs) " (" + ::vr::print (lhs) + ") != " VR_TO_STRING (rhs) + " (" + ::vr::print (rhs) + ")" \
            vr_CHECK_expand_context (__VA_ARGS__) ); } while (false) \
    /* */

#define check_lt(lhs, rhs, /* extra context */...) \
    do { if (VR_UNLIKELY (! (vr_lt (lhs, rhs)))) \
        throw_x (::vr::invalid_input, "failed: expected " VR_TO_STRING (lhs) " (" + ::vr::print (lhs) + ") < " VR_TO_STRING (rhs) + " (" + ::vr::print (rhs) + ")" \
            vr_CHECK_expand_context (__VA_ARGS__) ); } while (false) \
    /* */

#define check_le(lhs, rhs, /* extra context */...) \
    do { if (VR_UNLIKELY (! (vr_le (lhs, rhs)))) \
        throw_x (::vr::invalid_input, "failed: expected " VR_TO_STRING (lhs) " (" + ::vr::print (lhs) + ") <= " VR_TO_STRING (rhs) + " (" + ::vr::print (rhs) + ")" \
            vr_CHECK_expand_context (__VA_ARGS__) ); } while (false) \
    /* */

#define check_gt(lhs, rhs, /* extra context */...) \
    do { if (VR_UNLIKELY (! (vr_gt (lhs, rhs)))) \
        throw_x (::vr::invalid_input, "failed: expected " VR_TO_STRING (lhs) " (" + ::vr::print (lhs) + ") > " VR_TO_STRING (rhs) + " (" + ::vr::print (rhs) + ")" \
            vr_CHECK_expand_context (__VA_ARGS__) ); } while (false) \
    /* */

#define check_ge(lhs, rhs, /* extra context */...) \
    do { if (VR_UNLIKELY (! (vr_ge (lhs, rhs)))) \
        throw_x (::vr::invalid_input, "failed: expected " VR_TO_STRING (lhs) " (" + ::vr::print (lhs) + ") >= " VR_TO_STRING (rhs) + " (" + ::vr::print (rhs) + ")" \
            vr_CHECK_expand_context (__VA_ARGS__) ); } while (false) \
    /* */



#define check_null(v, /* extra context */...) \
    do { if (VR_UNLIKELY (! (vr_is_null (v)))) \
        throw_x (::vr::invalid_input, "failed: expected null " VR_TO_STRING (v) " (" + ::vr::print (v) + ")" \
            vr_CHECK_expand_context (__VA_ARGS__) ); } while (false) \
    /* */

#define check_nonnull(v, /* extra context */...) \
    do { if (VR_UNLIKELY (! (vr_is_nonnull (v)))) \
        throw_x (::vr::invalid_input, "failed: expected nonnull " VR_TO_STRING (v) \
            vr_CHECK_expand_context (__VA_ARGS__) ); } while (false) \
    /* */


#define check_addr_contained(addr, addr_a, addr_b, /* extra context */...) \
    do { if (VR_UNLIKELY (! (vr_is_addr_contained (addr, addr_a, addr_b)))) \
        throw_x (::vr::bad_address, "failed: expected " VR_TO_STRING (addr) " (" + ::vr::print (static_cast<addr_const_t> (addr)) + ")" \
            " in [" + ::vr::print (static_cast<addr_const_t> (addr_a)) + ", " + ::vr::print (static_cast<addr_const_t> (addr_b)) + "]" \
                vr_CHECK_expand_context (__VA_ARGS__) ); } while (false) \
    /* */

#define check_addr_range_contained(addr, addr_length, base, base_length, /* extra context */...) \
    do { if (VR_UNLIKELY (! (vr_is_addr_range_contained (addr, addr_length, base, base_length)))) \
        throw_x (::vr::bad_address, "failed: expected [" VR_TO_STRING (addr) ", " VR_TO_STRING (addr) " + " VR_TO_STRING (addr_length) "]" \
            " ([" + ::vr::print (static_cast<addr_const_t> (addr)) + ", " + ::vr::print (static_cast<addr_const_t> (byte_ptr_cast (addr) + addr_length)) + "])" \
            " in [" + ::vr::print (static_cast<addr_const_t> (base)) + ", " + ::vr::print (static_cast<addr_const_t> (byte_ptr_cast (base) + base_length)) + "]" \
            vr_CHECK_expand_context (__VA_ARGS__) ); } while (false) \
    /* */


#define check_empty(c, /* extra context */...) \
    do { if (VR_UNLIKELY (! (vr_is_empty (c)))) \
        throw_x (::vr::invalid_input, "failed: expected empty " VR_TO_STRING (c) " (" + ::vr::print (c) + ")" \
            vr_CHECK_expand_context (__VA_ARGS__) ); } while (false) \
    /* */

#define check_nonempty(c, /* extra context */...) \
    do { if (VR_UNLIKELY (! (vr_is_nonempty (c)))) \
        throw_x (::vr::invalid_input, "failed: expected nonempty " VR_TO_STRING (c) \
            vr_CHECK_expand_context (__VA_ARGS__) ); } while (false) \
    /* */

//............................................................................

// evaluated in debug builds only:

#if !defined (NDEBUG)

#   define assert_condition(condition, ...)             check_condition(condition, __VA_ARGS__)

#   define assert_within(i, limit, ...)                 check_within(i, limit, __VA_ARGS__)
#   define assert_within_inclusive(i, limit, ...)       check_within_inclusive(i, limit, __VA_ARGS__)
#   define assert_in_range(i, a, b, ...)                check_in_range(i, a, b, __VA_ARGS__)
#   define assert_in_inclusive_range(i, a, b, ...)      check_in_inclusive_range(i, a, b, __VA_ARGS__)
#   define assert_in_exclusive_range(i, a, b, ...)      check_in_exclusive_range(i, a, b, __VA_ARGS__)
#   define assert_is_power_of_2(i, ...)                 check_is_power_of_2(i, __VA_ARGS__)

#   define assert_positive(v, ...)                      check_positive(v, __VA_ARGS__)
#   define assert_nonnegative(v, ...)                   check_nonnegative(v, __VA_ARGS__)
#   define assert_zero(v, ...)                          check_zero(v, __VA_ARGS__)
#   define assert_nonzero(v, ...)                       check_nonzero(v, __VA_ARGS__)

#   define assert_null(p, ...)                          check_null(p, __VA_ARGS__)
#   define assert_nonnull(p, ...)                       check_nonnull(p, __VA_ARGS__)

#   define assert_empty(c, ...)                         check_empty(c, __VA_ARGS__)
#   define assert_nonempty(c, ...)                      check_nonempty(c, __VA_ARGS__)

#   define assert_addr_contained(addr, addr_a, addr_b, ...) \
                                                        check_addr_contained(addr, addr_a, addr_b, __VA_ARGS__)

#   define assert_addr_range_contained(addr, addr_length, base, base_length, ...) \
                                                        check_addr_range_contained(addr, addr_length, base, base_length, __VA_ARGS__)

#   define assert_eq(lhs, rhs, ...)                     check_eq(lhs, rhs, __VA_ARGS__)
#   define assert_ne(lhs, rhs, ...)                     check_ne(lhs, rhs, __VA_ARGS__)
#   define assert_lt(lhs, rhs, ...)                     check_lt(lhs, rhs, __VA_ARGS__)
#   define assert_le(lhs, rhs, ...)                     check_le(lhs, rhs, __VA_ARGS__)
#   define assert_gt(lhs, rhs, ...)                     check_gt(lhs, rhs, __VA_ARGS__)
#   define assert_ge(lhs, rhs, ...)                     check_ge(lhs, rhs, __VA_ARGS__)

#else // elided in release builds:

#   define assert_condition(condition, ...)

#   define assert_within(i, limit, ...)
#   define assert_within_inclusive(i, limit, ...)
#   define assert_in_range(i, a, b, ...)
#   define assert_in_inclusive_range(i, a, b, ...)
#   define assert_in_exclusive_range(i, a, b, ...)
#   define assert_is_power_of_2(i, ...)

#   define assert_positive(v, ...)
#   define assert_nonnegative(v, ...)
#   define assert_zero(v, ...)
#   define assert_nonzero(v, ...)

#   define assert_null(p, ...)
#   define assert_nonnull(p, ...)

#   define assert_empty(c, ...)
#   define assert_nonempty(c, ...)

#   define assert_addr_contained(addr, addr_a, addr_b, ...)
#   define assert_addr_range_contained(addr, addr_length, base, base_length, ...)

#   define assert_eq(lhs, rhs, ...)
#   define assert_ne(lhs, rhs, ...)
#   define assert_lt(lhs, rhs, ...)
#   define assert_le(lhs, rhs, ...)
#   define assert_gt(lhs, rhs, ...)
#   define assert_ge(lhs, rhs, ...)

#endif // NDEBUG

//----------------------------------------------------------------------------
