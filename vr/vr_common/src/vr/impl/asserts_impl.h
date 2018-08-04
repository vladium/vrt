#pragma once

#include "vr/preprocessor.h" // VR_TO_STRING

//----------------------------------------------------------------------------

#define vr_CHECK_emit_exp_list(is_empty, exp_list) \
    BOOST_PP_IF (is_empty, BOOST_PP_EMPTY, BOOST_PP_IDENTITY (" {"))() \
    BOOST_PP_LIST_FOR_EACH_I (vr_CHECK_emit_exp, unused, exp_list) \
    BOOST_PP_IF (is_empty, BOOST_PP_EMPTY, BOOST_PP_IDENTITY (+ '}'))() \
    /* */

#   define vr_CHECK_emit_exp(r, unused, i, exp) \
        + std::string { BOOST_PP_IF (i, BOOST_PP_IDENTITY (", "), BOOST_PP_EMPTY)() VR_TO_STRING (exp) ": " } + ::vr::print ((exp)) \
        /* */

//............................................................................

#define vr_CHECK_expand_context(...) \
    vr_CHECK_emit_exp_list (BOOST_VMD_IS_EMPTY (__VA_ARGS__), VR_VARIADIC_TO_LIST (__VA_ARGS__))

//----------------------------------------------------------------------------
