#pragma once

#include <boost/preprocessor/control/if.hpp>
#include <boost/preprocessor/facilities/empty.hpp>
#include <boost/preprocessor/facilities/identity.hpp>
#include <boost/preprocessor/list/for_each_i.hpp>
#include <boost/preprocessor/stringize.hpp>
#include <boost/preprocessor/variadic/to_list.hpp>
#include <boost/vmd/is_empty.hpp>

//----------------------------------------------------------------------------

#if !defined (VR_VARIADIC_TO_LIST) // don't want to pull in all of "preprocessor.h" and deal with inclusion cycles

#   define VR_VARIADIC_TO_LIST(...) \
        BOOST_PP_IF (BOOST_VMD_IS_EMPTY (__VA_ARGS__), BOOST_PP_EMPTY (), BOOST_PP_VARIADIC_TO_LIST (__VA_ARGS__)) \
        /* */
#endif
//............................................................................

#define vr_ASSUME_UNREACHABLE_emit_exp_list(is_empty, exp_list) \
    BOOST_PP_IF (is_empty, BOOST_PP_EMPTY, BOOST_PP_IDENTITY (" {"))()          \
    BOOST_PP_LIST_FOR_EACH_I (vr_ASSUME_UNREACHABLE_emit_exp, unused, exp_list) \
    BOOST_PP_IF (is_empty, BOOST_PP_EMPTY, BOOST_PP_IDENTITY (+ '}'))()         \
    /* */

#   define vr_ASSUME_UNREACHABLE_emit_exp(r, unused, i, exp) \
        + std::string { BOOST_PP_IF (i, BOOST_PP_IDENTITY (", "), BOOST_PP_EMPTY)() BOOST_PP_STRINGIZE (exp) ": " } + ::vr::print ((exp)) \
        /* */

//............................................................................

#define vr_ASSUME_UNREACHABLE_expand_args(...) \
    vr_ASSUME_UNREACHABLE_emit_exp_list (BOOST_VMD_IS_EMPTY (__VA_ARGS__), VR_VARIADIC_TO_LIST (__VA_ARGS__))

//----------------------------------------------------------------------------
