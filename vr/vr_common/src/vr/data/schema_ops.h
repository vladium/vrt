#pragma once

#include "vr/data/attributes_fwd.h"
#include "vr/data/defs.h"
#include "vr/meta/integer.h"
#include "vr/util/ops_int.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace data
{
//............................................................................
//............................................................................
namespace impl
{

using uwidth_type               = std::make_unsigned<width_type>::type;

//............................................................................


template<typename BITSET>
struct attr_offset
{
    vr_static_assert (std::is_unsigned<BITSET>::value);

    static constexpr int32_t slot_width ()  { return (8 * sizeof (BITSET)); }
    static constexpr int32_t slot_shift ()  { return meta::static_log2_floor<slot_width ()>::value; }

    /*
     * single bitmap slot
     */
    static int32_t evaluate (BITSET const bitmap, int32_t const index)
    {
        assert_within (index, slot_width ());

        BITSET bit = (static_cast<BITSET> (1) << index);

        int32_t const popcnt_lt_index   = util::popcount (bitmap & (bit - 1));
        int32_t const popcnt_all        = util::popcount (bitmap);

        int32_t const is_zero_mask      = - ! (bitmap & bit);

        return ((is_zero_mask & (2 * popcnt_all + index - popcnt_lt_index)) | (~ is_zero_mask & (2 * popcnt_lt_index))); // branchless
    }

    /*
     * an array of {cumulative offset, bitmap} pairs
     */
    static BITSET evaluate (BITSET const VR_RESTRICT * const offset_map, width_type const index, width_type const log2_length)
    {
        assert_nonnull (offset_map);

        width_type const entry_index    = ((static_cast<uwidth_type> (index) >> slot_shift ()) << 1); // each entry is two consecuitive array slots
        int32_t const bitmap_index      = (static_cast<uint32_t> (index) & (slot_width () - 1));

        return (offset_map [entry_index] + (evaluate (offset_map [entry_index + 1], bitmap_index) << log2_length));
    }

}; // end of class

} // end of 'impl'
//............................................................................
//............................................................................


} // end of 'data'
} // end of namespace
//----------------------------------------------------------------------------
