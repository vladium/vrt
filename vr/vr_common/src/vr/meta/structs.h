#pragma once

#include "vr/meta/impl/structs_impl.h"
#include "vr/meta/tags.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace meta
{
//............................................................................
/**
 * A macro invocation like this:
 *@code
 *      VR_META_PACKED_STRUCT
 *      (S,
 *          (char,          f0)
 *          (int64_t,       f1)
 *          (fw_string4,    f2)
 *      );
 *@endcode
 * generates a packed ("overlay") struct <em>S</em> with the following structure
 * in the defining namespace:
 *
 *  - TODO finish doc
 */
#define VR_META_PACKED_STRUCT(.../* cls[, base], field_seq */) \
    BOOST_PP_CAT (vr_META_PACKED_STRUCT_, BOOST_PP_VARIADIC_SIZE (__VA_ARGS__)) (__VA_ARGS__) \
    /* */

//............................................................................

#define VR_META_COMPACT_STRUCT(.../* cls[, base], field_seq */) \
    BOOST_PP_CAT (vr_META_COMPACT_STRUCT_, BOOST_PP_VARIADIC_SIZE (__VA_ARGS__)) (__VA_ARGS__) \
    /* */

//............................................................................

} // end of 'meta'
} // end of namespace
//----------------------------------------------------------------------------
