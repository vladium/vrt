#pragma once

#include "vr/meta/objects.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
/**
 */
template<typename T, typename = void>
struct persist_traits
{
    static_assert (util::lazy_false<T>::value, "'T' is not a meta struct and is therefore not persistable");

}; // end of master

template<typename T>
struct persist_traits<T,
    util::enable_if_t<meta::is_meta_struct<T>::value>>
{
    /**
     * default to the first [declared] meta field; specialize to override
     */
    using ID_type           = typename bmp::mp_front<meta::fields_of_t<T>>::tag;

}; // end of master

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
