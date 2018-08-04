#pragma once

#include "vr/fw_string.h"
#include "vr/market/defs.h"
#include "vr/util/ops_int.h"    // net_int

#include <cstring>

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
//............................................................................
// generic/common field types:

template<std::size_t SIZE>
using alphanum_ft           = alphanum<SIZE>;

using be_int16_ft           = util::net_int<int16_t, false>; // big-endian
using be_int32_ft           = util::net_int<int32_t, false>; // big-endian
using be_int64_ft           = util::net_int<int64_t, false>; // big-endian

template<typename T>
using sides                 = std::array<T, side::size>;

//............................................................................

using mold_seqnum_t         = int64_t;
using mold_seqnum_ft        = util::net_int<mold_seqnum_t, false>; // big-endian

//............................................................................

template<char FILL, std::size_t SIZE>
inline void
fill_alphanum (std::array<char, SIZE> & dst)
{
    __builtin_memset (dst.data (), FILL, SIZE);
}
//............................................................................

template<std::size_t SIZE>
inline void
copy_to_alphanum (std::string const & src, std::array<char, SIZE> & dst)
{
    assert_le (src.size (), SIZE);

    std::memcpy (dst.data (), src.data (), src.size ());
}

template<std::size_t N, std::size_t SIZE>
inline void
copy_to_alphanum (fw_string<N> const & src, std::array<char, SIZE> & dst) // avoid a std::string intermediary
{
    assert_le (static_cast<std::size_t> (src.size ()), SIZE);

    std::memcpy (dst.data (), src.data (), src.size ());
}

} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
