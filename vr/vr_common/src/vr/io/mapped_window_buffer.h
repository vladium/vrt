#pragma once

#include "vr/arg_map.h"
#include "vr/io/mapped_files.h"
#include "vr/util/logging.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
/**
 * @see mapped_tape_buffer
 *
 * this is like a more VM-economical version of @ref mapped_tape_buffer, in the
 * sense that it will slide and update its mmapping to tightly support only the bytes
 * currently in the buffer plus the w_window "reserve" in front of the write-position;
 *
 * this is similar to what @ref mapped_ofile does (on which this implementation is
 * in fact based) except it offers a stream-like interface, not random repositioning,
 * and fixes w_window at construction time;
 *
 * the price for less VM usage is mmap ops that can occur post construction; these
 * are done as rarely as possible, by delaying them until required and then sliding
 * the mapping window forward as much as possible; a pattern of N-sized writes (w_advance())
 * followed by their consumption via r_advance() will cause remapping approximately once
 * every capacity/N writes;
 *
 * @note a side-effect of this design that makes the API somewhat hard to use
 *       without errors is that w_advance() can not only change the write position
 *       address but also the read position address (any copy cached by the user
 *       thus gets invalidated); the safe approach is to always re-query r_position()
 *       (as done by the links, for example) or switch to a random-access API like @see mapped_ofile
 *
 * a good use case is data I/O in highly concurrent HPC environment, where VM
 * is a resource shared between many data processing tasks;
 *
 * another use case is data capture where it is desirable to force-flush data to disk
 * promptly and where the max size of arriving data packets is known (<= w_window);
 *
 *  - capacity: changes dynamically to retain all data plus w_window, but never smaller
 *              than the 'capacity' passed into the constructor;
 *  - size: dynamic and unbounded, i.e. the count of bytes currently in the buffer
 *          (the unconsumed bytes between the [conceptual] write- and read-positions);
 *  - w_window: finite, fixed at construction time
 *
 *  - invariant(s): there are at least 'w_window' bytes of mapped memory following the write position;
 *                  r_position <= w_position (but NOT necessarily monotonically increasing memory addresses over time);
 *
 *  - data stream persistent: yes
 *
 * @see mapped_ofile
 */
class mapped_window_buffer // TODO if mapped_ofile is made movable, make this movable as well
{
    public: // ...............................................................

        using size_type             = pos_t;

        /**
         * @param capacity [must be >= 'w_window']
         * @param w_window [must be <= 'capacity']
         */
        mapped_window_buffer (fs::path const & file, size_type const capacity, size_type const w_window,
                              pos_t const reserve_size = default_mmap_reserve_size (), clobber::enum_t const cm = clobber::error);
        mapped_window_buffer (arg_map const & args);

        ~mapped_window_buffer () VR_NOEXCEPT; // TODO doc how truncate/close is done


        // ACCESSORs:

        addr_const_t r_position () const
        {
             return addr_plus (m_out_w, m_r_offset);
        }

        /**
         * @return count of bytes available for reading from current @ref r_position()
         */
        VR_FORCEINLINE size_type size () const
        {
            auto const r = m_w_offset - m_r_offset;
            assert_nonnegative (r, m_r_offset, m_w_offset);

            return r;
        }

        /**
         * @return current capacity [no smaller than 'capacity' passed into constructor]
         */
        size_type const & capacity () const
        {
            return m_capacity;
        }

        /**
         * @return count of bytes mapped for writing from current @ref w_position(), as passed into the constructor
         */
        size_type const & w_window () const
        {
            return m_w_window;
        }

        // MUTATORs:

        addr_t w_position ()
        {
            return addr_plus (m_out_w, m_w_offset);
        }

        /**
         * @param step [must be in [0, size()] range] indicates how many bytes available to read to consume
         * @return new read position TODO doc that w pos is guaranteed not to have changed
         */
        template<bool CHECK_BOUNDS = VR_CHECK_INPUT>
        addr_const_t r_advance (size_type const step)
        {
            DLOG_trace2 << "[size: " << size () << ", capacity: " << m_capacity << "] r_advance (" << step << ')';

            if (CHECK_BOUNDS)
            {
                auto const sz = size ();
                check_within_inclusive (step, sz);
            }

            return addr_plus (m_out_w, m_r_offset += step);
        }

        /**
         * @param step [must be nonnegative] indicates by how many bytes to advance the write position;
         *        this does not discard any bytes available to read
         * @return new write position TODO doc that r pos may have changed as well
         */
        template<bool CHECK_BOUNDS = VR_CHECK_INPUT>
        addr_t w_advance (size_type const step)
        {
            DLOG_trace2 << "[size: " << size () << ", capacity: " << m_capacity << "] w_advance (" << step << ')';

            if (CHECK_BOUNDS) check_within_inclusive (step, m_w_window);

            auto const w_offset = m_w_offset + step;
            if (VR_LIKELY (w_offset <= m_capacity)) // fast no-remap case
                return addr_plus (m_out_w, m_w_offset = w_offset);

            return w_remap_advance (step); // slow case
        }

    private: // ..............................................................

        VR_ASSUME_HOT addr_t w_remap_advance (size_type const step);
        void truncate_and_close (); // TODO allow this to be public?

        // TODO switching to an equivalent impl with 'm_r_addr' and 'm_w_addr' would make r/w_position() faster, single field reads

        size_type const m_w_window;
        addr_t m_out_w { }; // memoized 'm_out' position
        size_type m_r_offset { };
        size_type m_w_offset { };
        size_type m_capacity { };
        mapped_ofile m_out;

}; // end of class

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
