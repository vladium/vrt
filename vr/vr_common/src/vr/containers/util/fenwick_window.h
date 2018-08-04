#pragma once

#include "vr/meta/arrays.h"
#include "vr/meta/integer.h" // boost::static_log2
#include "vr/sys/cpu.h"
#include "vr/util/logging.h"
#include "vr/util/type_traits.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{

template
<
    std::size_t WINDOW      = (1L << 30), // in whatever units are meaningful for 'T'
    typename BIN_TYPE       = int16_t,
    int32_t BIN_COUNT       = (2 * sys::cpu_info::cache::static_line_size () / sizeof (BIN_TYPE)) // [heuristic default] number of bins in window
>
struct fenwick_window_options
{
    static constexpr std::size_t window     = WINDOW;
    using bin_type                          = BIN_TYPE;
    static constexpr int32_t bin_count      = BIN_COUNT;

}; // end of traits
//............................................................................
// TODO
// - memoized impl, perhaps enabled by a trait?
/**
 * @cite FTs
 */
template<typename T, typename OPTIONS = fenwick_window_options<>>
class fenwick_window final
{
    private: // ..............................................................

        vr_static_assert (std::is_signed<T>::value);
        using T_unsigned    = std::make_unsigned_t<T>;

        using bin_type      = typename OPTIONS::bin_type;
        vr_static_assert (std::is_signed<bin_type>::value);

        // TODO if 'bin_type' is narrow, guard against numeric overflow in bins in debug build

    public: // ...............................................................

        using options           = OPTIONS;
        using window_count_type = int32_t;

        static constexpr std::size_t window ()  { return OPTIONS::window; }
        static constexpr int32_t bin_count ()   { return OPTIONS::bin_count; }


        fenwick_window (T const start_time) :
            m_time_index { static_cast<T> (static_cast<T_unsigned> (start_time) >> log2_bin_width ()) },
            m_front_window { (static_cast<uint32_t> (m_time_index) & _2_window_bin_mask ()) >= bin_count () }
        {
            check_nonnegative (start_time);
        }

        // ACCESSORs:

        /**
         * @return running window count as of the last @ref add() time
         */
        window_count_type count () const;

        // MUTATORs:

        /**
         * advance window right edge to 'time', add thusly-stamped 'increment', and return
         * the resulting window count
         *
         * @note in (rare cases) when the increment must be "rolled back", this can
         *       be effected by doing 'add(same time, - increment)'; however, this will only be
         *       an approximate rollback since the window edge advancement won't be undone
         *       (it progresses monotonically; this is by design); in other words, the undone
         *       count will in general be lower than might be expected by subtracting 'increment'
         *       due to some old event dropping out of the window;
         *
         *       [and of course such "rollback" must follow the add() it is trying to roll back
         *
         * @return updated count (inclusive of 'increment' and 'time' progression)
         */
        template<bool CHECK_MONOTONICITY = true>
        VR_ASSUME_HOT window_count_type add (T const time, int32_t const increment = 1);

    private: // ..............................................................

        vr_static_assert (vr_is_power_of_2 (window ()));
        vr_static_assert (vr_is_power_of_2 (bin_count ()));

        using window_data   = std::array<bin_type, bin_count ()>;

        // masks and shifts used with 'T' values:

        static constexpr int32_t log2_bin_width ()      { return boost::static_log2<(window () / bin_count ())>::value; }

        // masks and shifts used with bin index values:

        static constexpr uint32_t _window_bin_mask ()   { return (bin_count () - 1); }
        static constexpr uint32_t _2_window_bin_mask () { return (2 * bin_count () - 1); }


        VR_FORCEINLINE window_count_type count_impl (int32_t r, bool const front_window) const;

        T m_time_index; // last addition "time" [in bin units]
        bool m_front_window;
        std::array<window_data, 2> m_window
        {
            meta::create_array_fill<bin_type, bin_count (), 0> (),
            meta::create_array_fill<bin_type, bin_count (), 0> ()
        };

}; // end of class
//............................................................................

template<typename T, typename OPTIONS>
typename fenwick_window<T, OPTIONS>::window_count_type
fenwick_window<T, OPTIONS>::count_impl (int32_t r, bool const front_window) const
{
    assert_within_inclusive (r, bin_count ());

    window_data const & fw = m_window [front_window];
    window_data const & bw = m_window [! front_window];

    // count = cumsum (front_window) + [totalsum (back_window) - cumsum (back_window)],
    // where:
    //  - totalsum (back_window) is always the count in its last bin;
    //  - cumsum (front_window) and cumsum (back_window) follow the same index sequences
    //    and hence can be fused into the same loop;

    window_count_type sum = bw [bin_count () - 1]; // start with 'totalsum (back_window)'

    // add 'cumsum (front_window) - cumsum (back_window)':

    while (r > 0) // O(log2(r))
    {
        sum += (fw [r - 1] - bw [r - 1]); // TODO widen these first, to speed up arithmetic?
        r ^= (r & - r); // turn off the rightmost 1-bit of 'r'
    }

    return sum;
}
//............................................................................

template<typename T, typename OPTIONS>
typename fenwick_window<T, OPTIONS>::window_count_type
fenwick_window<T, OPTIONS>::count () const
{
    int32_t r = (static_cast<uint32_t> (m_time_index) & _2_window_bin_mask ()); // current window right edge

    DLOG_trace1 << "count: time index: " << m_time_index << ", r: " << r << ", front window: " << m_front_window;

    r = (r & _window_bin_mask ()) + 1; // shift to single-window indexing and adjust for 0-based 'window_data' access

    return count_impl (r, m_front_window);
}
//............................................................................

template<typename T, typename OPTIONS>
template<bool CHECK_MONOTONICITY>
typename fenwick_window<T, OPTIONS>::window_count_type
fenwick_window<T, OPTIONS>::add (T const time, int32_t const increment)
{
    assert_nonnegative (time);
    assert_nonzero (increment);

    T const time_index = static_cast<T> (static_cast<T_unsigned> (time) >> log2_bin_width ());

    int32_t r = (static_cast<uint32_t> (time_index) & _2_window_bin_mask ()); // new window right edge
    bool const front_window = (r >= bin_count ());

    r = (r & _window_bin_mask ()) + 1; // shift to single-window indexing and adjust for 0-based 'window_data' access

    window_data * fw; // set by one of the 3 cases below

    if (time_index >= m_time_index + static_cast<T> (window () >> log2_bin_width ()))
    {
        // reset windows:

        // 'time' is more than a window later than the last update, old history is irrelevant;
        // clear both windows and update the new front with 'increment':

        DLOG_trace1 << "window reset";

        __builtin_memset (& m_window [0], 0, 2 * sizeof (window_data)); // TODO hint alignment of pointer

        fw = & m_window [m_front_window = front_window];
    }
    else
    {
        if (CHECK_MONOTONICITY) check_le (m_time_index, time_index, time);

        bool const cur_front_window = m_front_window;

        if (front_window != cur_front_window)
        {
            // swap windows:

            DLOG_trace1 << "window roll";

            // front window becomes the new back, but its data needs no changes;
            // back window becomes the new front, needs to be cleared and updated with 'increment':

            fw = & m_window [m_front_window = front_window];

            __builtin_memset (fw, 0, sizeof (window_data)); // TODO hint alignment of pointer
        }
        else // no window roll, so only need to update the front window:
        {
            DLOG_trace2 << "front window update";

            fw = & m_window [cur_front_window];
        }
    }

    int32_t u = r; // retain 'r' for the count_impl() call below

    while (u <= bin_count ()) // O(log2(bin_count - u))
    {
        (* fw) [u - 1] += increment;
        u += (u & - u);
    }

    m_time_index = time_index;

    return count_impl (r, front_window);
}

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
