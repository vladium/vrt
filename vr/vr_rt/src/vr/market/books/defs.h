#pragma once

#include "vr/meta/objects.h" // empty_struct
#include "vr/meta/tags.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
//............................................................................

VR_META_TAG (book);

VR_META_TAG (depth);
VR_META_TAG (order_count);
VR_META_TAG (order_queue);

//............................................................................
//............................................................................
namespace impl_common
{

struct user_data_mark   { }; // base for finding 'user_data'

using empty_ud          = meta::empty_struct;

template<typename T>
struct user_data: public user_data_mark
{
    using user_type         = util::if_is_void_t<T, empty_ud, T>;

}; // end of tag

using default_user_data     = user_data<void>;

} // end of 'impl_common'
//............................................................................
//............................................................................
/**
 * an optional attribute to describe custom data type 'T' to be associated
 * with an individual oid limit/execution order in books structures
 * like 'execution_order_book' or 'limit_order_book'
 *
 * @see execution_order_book
 * @see limit_order_book
 */
template<typename /* end-user provided, must be default-constructible */T = void>
using user_data             = impl_common::user_data<T>;

} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
