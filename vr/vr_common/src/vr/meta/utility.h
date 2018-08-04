#pragma once

#include "vr/enums.h"
#include "vr/util/type_traits.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace meta
{
//............................................................................

template<typename T, typename = void>
struct storage_size_of
{
    static constexpr std::size_t value  = sizeof (T);

}; // end of master

template<typename T>    // void
struct storage_size_of<T,
    util::enable_if_t<std::is_same<T, void>::value>>
{
    static constexpr std::size_t value  = 0;

}; // end of specialization

template<typename T>    // typesafe enum
struct storage_size_of<T,
    util::enable_if_t<is_typesafe_enum<T>::value>>
{
    static constexpr std::size_t value  = sizeof (typename T::enum_t);

}; // end of specialization

template<typename T>    // empty
struct storage_size_of<T,
    util::enable_if_t<(std::is_empty<T>::value && ! is_typesafe_enum<T>::value)>>
{
    static constexpr std::size_t value  = 0;

}; // end of specialization
//............................................................................

template<std::size_t SIZE, typename T = int8_t>
struct padding
{
    T _ [SIZE];

}; // end of master

template<typename T>
struct padding<0, T>
{
}; // end of specialization
//............................................................................
/*
 * a utility wrapper to avoid forming 'void &' expressions that trigger compile-time errors
 */
template<typename T>
struct safe_referencable
{
    using type              = util::if_is_void_t<T, addr_const_t, T>;

}; // end of metafunction

template<typename T>
using safe_referencable_t   = typename safe_referencable<T>::type;

} // end of 'meta'
} // end of namespace
//----------------------------------------------------------------------------
