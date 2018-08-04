#pragma once

#include "vr/config.h" // _GNU_SOURCE

#include "vr/util/strong_typedef.h" // VR_STRONG_ALIAS

#include <boost/call_traits.hpp>
#include <boost/dynamic_bitset.hpp>
#include <boost/noncopyable.hpp>
#include <boost/unordered_map.hpp>
#include <boost/unordered_set.hpp>

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <string>
#include <type_traits>
#include <vector>

//----------------------------------------------------------------------------
namespace vr
{
//............................................................................

using std::int64_t;
using std::int32_t;
using std::int16_t;
using std::int8_t;

using std::uint64_t;
using std::uint32_t;
using std::uint16_t;
using std::uint8_t;

//............................................................................

using xmmi_t                = int64_t __attribute__ ((__vector_size__ (16)));

//............................................................................

using native_int128_t       = __int128;
using native_uint128_t      = unsigned __int128;

constexpr native_uint128_t _one_128 ()      { return static_cast<native_uint128_t> (1); }

//............................................................................

using std_intptr_t          = std::intptr_t;
using std_uintptr_t         = std::uintptr_t;

using addr_const_t          = void const *;
using addr_t                = void *;

using string_literal_t      = char const *; // 0-terminated
using char_const_ptr_t      = char const *; // not necessarily 0-terminated

using signed_size_t         = std::make_signed<std::size_t>::type;

using enum_int_t            = uint32_t; // note: unsigned by design (see 'data::NA' notes and 'enum_NA ()' below)

using bitset16_t            = uint16_t;
using bitset32_t            = uint32_t;
using bitset64_t            = uint64_t;
using bitset128_t           = native_uint128_t;

//............................................................................

using string_vector         = std::vector<std::string>;

//............................................................................

using bit_set               = boost::dynamic_bitset<uint64_t>;

template<typename SIZE_TYPE>
inline bit_set
make_bit_set (SIZE_TYPE const size)
{
    return bit_set { static_cast<bit_set::size_type> (size) }; // no bits set
}
//............................................................................

using timestamp_t           = int64_t;

constexpr timestamp_t _1_second ()      { return 1000000000L; }
constexpr timestamp_t _1_millisecond () { return    1000000L; }
constexpr timestamp_t _1_microsecond () { return       1000L; }
constexpr timestamp_t _1_nanosecond ()  { return          1L; }

//............................................................................

using noncopyable           = boost::noncopyable;

//............................................................................

template<typename T, std::size_t N>
constexpr std::size_t
length (T (&)[N])
{
    return N;
}
/*
 * c.f 'array_size' for std::array in "type_traits.h"
 */
template<typename T, std::size_t N>
struct c_array_type
{
    using type              = T (&) [N];
    using const_type        = T const (&) [N];

}; // end of metafunction
//............................................................................

using free_fn_t             = void (*)(addr_t); // std::free() signature

//............................................................................

template<typename T>
struct call_traits: public boost::call_traits<T>
{
    using param             = typename boost::call_traits<T>::param_type; // less typing

}; // end of metafunction
//............................................................................

constexpr enum_int_t enum_NA () { return -1; }

} // end of namespace
//----------------------------------------------------------------------------
