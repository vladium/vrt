#pragma once

#include "vr/macros.h"

#include <boost/preprocessor/arithmetic/dec.hpp>
#include <boost/preprocessor/arithmetic/inc.hpp>
#include <boost/preprocessor/cat.hpp>
#include <boost/preprocessor/control/if.hpp>
#include <boost/preprocessor/facilities/empty.hpp>
#include <boost/preprocessor/facilities/identity.hpp>
#include <boost/preprocessor/list/for_each.hpp>
#include <boost/preprocessor/list/for_each_i.hpp>
#include <boost/preprocessor/variadic/size.hpp>
#include <boost/preprocessor/variadic/to_list.hpp>
#include <boost/vmd/is_empty.hpp>

//----------------------------------------------------------------------------

#if !defined (VR_TO_STRING)
#   define VR_TO_STRING(x)          BOOST_PP_STRINGIZE (x)
#endif

#define VR_CAT(x, y)                BOOST_PP_CAT (x, y)

//............................................................................

#define VR_INC(x)                   BOOST_PP_INC(x)
#define VR_DEC(x)                   BOOST_PP_DEC(x)

//............................................................................

#define VR_select_x(x, y)           x
#define VR_select_y(x, y)           y

#define VR_IF_THEN_ELSE(cond)/*(then, else)*/ \
                                    BOOST_PP_IF(cond, VR_select_x, VR_select_y)

//............................................................................
/**
 * handle empty variadic args during boost.preprocessor list conversion
 */
#define VR_VARIADIC_TO_LIST(...) \
    BOOST_PP_IF (BOOST_VMD_IS_EMPTY (__VA_ARGS__), BOOST_PP_EMPTY (), BOOST_PP_VARIADIC_TO_LIST (__VA_ARGS__)) \
    /* */
//............................................................................

#define VR_add_paren_2_A(x, y) ((x, y)) VR_add_paren_2_B
#define VR_add_paren_2_B(x, y) ((x, y)) VR_add_paren_2_A
#define VR_add_paren_2_A_END
#define VR_add_paren_2_B_END

/**
 * given input 'seq' of the form (a0, b0)(a1, b1)...
 * transform it into the form ((a0, b0))((a1, b1))...,
 *
 * which is useful for working with sequences of tuples.
 *
 * @see http://boost.2283326.n4.nabble.com/Preprocessor-sequence-of-quot-pairs-quot-td4636239.html
 */
#define VR_DOUBLE_PARENTHESIZE_2(seq) BOOST_PP_CAT (VR_add_paren_2_A seq, _END)

//----------------------------------------------------------------------------
