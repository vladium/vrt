#pragma once

#include "vr/asserts.h"

#include <boost/integer/static_log2.hpp>
#include <boost/integer/static_min_max.hpp>

//----------------------------------------------------------------------------
namespace vr
{
namespace meta
{
//............................................................................

template<std::size_t V>
struct static_is_power_of_2: util::bool_constant<((V > 0) & ! (V & (V - 1)))>
{
}; // end of metafunction
//............................................................................

template<std::size_t V>
struct static_log2_floor
{
    static constexpr int32_t value  = boost::static_log2<V>::value;

}; // end of metafunction


template<std::size_t V>
struct static_log2_ceil; // forward

template<>
struct static_log2_ceil<1>
{
    static constexpr int32_t value  = 0;

}; // end of specialization

template<std::size_t V>
struct static_log2_ceil
{
    static constexpr int32_t value  = 1 + static_log2_floor<(V - 1)>::value;

}; // end of specialization
//............................................................................

template<typename I, int32_t ... VALUEs>
struct static_bitset; // master

template<typename I>
struct static_bitset<I>
{
    static constexpr I value        = 0;

}; // end of specialization

template<typename I, int32_t V, int32_t ... VALUEs>
struct static_bitset<I, V, VALUEs ...>
{
    vr_static_assert (0 <= V && V < 8 * sizeof (I));

    static constexpr I value        = (static_cast<I> (1) << V) | static_bitset<I, VALUEs ...>::value;

}; // end of specialization
//............................................................................

template<int32_t ... VALUEs>
struct static_mux; // master

template<>
struct static_mux<>
{
    static constexpr bitset32_t value   = 0;

}; // end of specialization

template<int32_t V, int32_t ... VALUEs>
struct static_mux<V, VALUEs ...>
{
    vr_static_assert (sizeof ... (VALUEs) < sizeof (bitset32_t));
    vr_static_assert (0 <= V && V < 256);

    static constexpr bitset32_t value   = (static_cast<bitset32_t> (static_cast<uint8_t> (V)) << (8 * sizeof ... (VALUEs))) | static_mux<VALUEs ...>::value;

}; // end of specialization
//............................................................................
//............................................................................
namespace impl
{

template<typename I, bool I_IS_SIGNED, I ... VALUEs>
struct static_range_impl;

// unsigned:

template<typename I, I V>
struct static_range_impl<I, /* I_IS_SIGNED */false, V>
{
    static constexpr I min ()       { return V; }
    static constexpr I max ()       { return V; }

}; // end of specialization

template<typename I, I V, I ... VALUEs>
struct static_range_impl<I, /* I_IS_SIGNED */false, V, VALUEs ...>
{
    static constexpr I min ()       { return static_cast<I> (boost::static_unsigned_min<V, static_range_impl<I, false, VALUEs ...>::min ()>::value); }
    static constexpr I max ()       { return static_cast<I> (boost::static_unsigned_max<V, static_range_impl<I, false, VALUEs ...>::max ()>::value); }

}; // end of specialization

// signed:

template<typename I, I V>
struct static_range_impl<I, /* I_IS_SIGNED */true, V>
{
    static constexpr I min ()       { return V; }
    static constexpr I max ()       { return V; }

}; // end of specialization

template<typename I, I V, I ... VALUEs>
struct static_range_impl<I, /* I_IS_SIGNED */true, V, VALUEs ...>
{
    static constexpr I min ()       { return static_cast<I> (boost::static_signed_min<V, static_range_impl<I, true, VALUEs ...>::min ()>::value); }
    static constexpr I max ()       { return static_cast<I> (boost::static_signed_max<V, static_range_impl<I, true, VALUEs ...>::max ()>::value); }

}; // end of specialization

} // end of 'impl'
//............................................................................
//............................................................................

template<typename I, I ... VALUEs>
struct static_range
{
    vr_static_assert (sizeof ... (VALUEs) > 0);

    using limits        = impl::static_range_impl<I, std::is_signed<I>::value, VALUEs ...>;

    static constexpr I min ()           { return limits::min (); }
    static constexpr I max ()           { return limits::max (); }

}; // end of metafunction
//............................................................................
//............................................................................
namespace impl
{

template<typename V>
struct static_byteswap_impl; // master

// specializations:

template<>
struct static_byteswap_impl<uint16_t>
{
    static constexpr uint16_t evaluate (uint16_t const x)
    {
        return ((x << 8) | (x >> 8));
    }

}; // end of specialization

template<>
struct static_byteswap_impl<uint32_t>
{
    static constexpr uint32_t evaluate (uint32_t const x)
    {
        return ((static_cast<uint32_t> (static_byteswap_impl<uint16_t>::evaluate (x)) << 16) | static_byteswap_impl<uint16_t>::evaluate (x >> 16));
    }

}; // end of specialization

template<>
struct static_byteswap_impl<uint64_t>
{
    static constexpr uint64_t evaluate (uint64_t const x)
    {
        return ((static_cast<uint64_t> (static_byteswap_impl<uint32_t>::evaluate (x)) << 32) | static_byteswap_impl<uint32_t>::evaluate (x >> 32));
    }

}; // end of specialization

}
//............................................................................
//............................................................................

template<typename V>
constexpr typename std::make_unsigned<V>::type
static_byteswap (V const x)
{
    using V_unsigned                = typename std::make_unsigned<V>::type;

    return impl::static_byteswap_impl<V_unsigned>::evaluate (static_cast<V_unsigned> (x));
}
//............................................................................

template<typename V>
constexpr typename std::make_unsigned<V>::type
static_host_to_net (V const x)
{
    return static_byteswap (x);
}

} // end of 'meta'
} // end of namespace
//----------------------------------------------------------------------------
