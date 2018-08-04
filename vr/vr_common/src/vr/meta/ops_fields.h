#pragma once

#include "vr/meta/objects.h"
#include "vr/strings.h" // print_tag()
#include "vr/util/logging.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace meta
{
//............................................................................
//............................................................................
namespace impl
{

template<typename SRC, typename DST, typename ... TAGs>
struct copy_all_fields_impl; // master


template<typename SRC, typename DST>
struct copy_all_fields_impl<SRC, DST>
{
    static VR_FORCEINLINE void evaluate (SRC const & src, DST & dst)    { }

}; // end of specialization

template<typename SRC, typename DST, typename TAG, typename ... TAGs>
struct copy_all_fields_impl<SRC, DST, TAG, TAGs ...>
{
    static VR_FORCEINLINE void evaluate (SRC const & src, DST & dst)
    {
        vr_static_assert (has_field<TAG, SRC> ());
        vr_static_assert (has_field<TAG, DST> ());

        DLOG_trace1 << "  copying " << print_tag<TAG, true> ();

        field<TAG> (dst) = field<TAG> (src);

        copy_all_fields_impl<SRC, DST, TAGs ...>::evaluate (src, dst);
    }

}; // end of specialization

} // end of 'impl'
//............................................................................
//............................................................................
/**
 * @note copying is done via operator=
 *
 * @tparam TAGs fields must exist in BOTH 'SRC' and 'DST'
 */
template<typename ... TAGs> // TODO take an mp11 seq as well
struct ops_fields final
{
    template<typename SRC, typename DST>
    static VR_FORCEINLINE void copy_all (SRC const & src, DST & dst)
    {
        impl::copy_all_fields_impl<SRC, DST, TAGs ...>::evaluate (src, dst);
    }

}; // end of class

// TODO
//template<typename ... TAG>
//struct copy_all_lhs_fields; etc

} // end of 'meta'
} // end of namespace
//----------------------------------------------------------------------------
