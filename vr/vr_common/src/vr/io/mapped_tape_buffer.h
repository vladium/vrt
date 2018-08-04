#pragma once

#include "vr/arg_map.h"
#include "vr/filesystem.h"
#include "vr/io/defs.h"
#include "vr/util/logging.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
// TODO constr option for pre-faulting some of the pages (MAP_POPULATE or similar)
// TODO constr option for mapping via huge pages
/**
 * a buffer implementation that creates a large ("infinite") file that is
 * mapped into VM at construction time and will grow its actual disk image
 * usage only as data is written into it;
 *
 * like @ref mapped_window_buffer, this implementation persists its data stream;
 * unlike @ref mapped_window_buffer, this implementation has effectively infinite
 * capacity and w_window but can (in the current version) result in large VM
 * utilization (depending on the kernel's policy for flushing dirty pages to disk);
 *
 * this buffer does not do any mmap or heap allocation ops after construction
 * but will continue to fault-in new pages as the read/write-positions advance
 * forward; at the moment, the pages that have been "consumed" (i.e. are behind
 * the read-position) and not forcibly unmmap'ed and the "future" pages (ahead
 * of the write-position) are not pre-faulted ahead of time -- but this could
 * change;
 *
 * a reasonable use case is data capture, if the amount of data is a comfortably
 * small fraction of the available physical memory;
 *
 *  - capacity: "infinite" ('reserve_size'), fixed at construction time;
 *  - size: dynamic in [0, capacity] range, i.e. the count of bytes currently in the buffer
 *          (the unconsumed bytes between the write- and read-positions);
 *  - w_window: "infinite" (min ('reserve_size' - size, max window_t representable value));
 *
 *  - invariant(s): size + w_window == capacity,
 *                  r_position <= w_position and both are monotonically increasing memory addresses over time
 *
 *  - data stream persistent: yes
 *
 *  @see mapped_window_buffer
 *  @see mapped_ofile
 */
class mapped_tape_buffer // movable, but not assignable/copyable
{
    public: // ...............................................................

        using size_type             = pos_t;

        // TODO option to map privately

        mapped_tape_buffer (fs::path const & file, pos_t const reserve_size = default_mmap_reserve_size (), clobber::enum_t const cm = clobber::error);
        mapped_tape_buffer (arg_map const & args);
        ~mapped_tape_buffer () VR_NOEXCEPT;

        mapped_tape_buffer (mapped_tape_buffer && rhs);


        // ACCESSORs:

        addr_const_t r_position () const
        {
             return m_r_addr;
        }

        /**
         * @return count of bytes available for reading from current @ref r_position()
         *
         * @invariant size() + w_window() <= capacity()
         */
        VR_FORCEINLINE size_type size () const
        {
            auto const sz = intptr (m_w_addr) - intptr (m_r_addr);

            assert_within_inclusive (sz, capacity ());

            return sz;
        }

        /**
         * @invariant size() + w_window() <= capacity()
         */
        VR_FORCEINLINE size_type capacity () const
        {
            return m_reserve_size;
        }

        /**
         * @return count of bytes available for writing from current @ref w_position()
         *
         * @invariant size() + w_window() <= capacity()
         */
        VR_FORCEINLINE size_type w_window () const
        {
            // checked by 'size()': assert_within_inclusive (m_w_addr - m_r_addr, capacity ());

            return (capacity () - size ());
        }

        // MUTATORs:

        addr_t w_position ()
        {
            return m_w_addr;
        }

        template<bool CHECK_BOUNDS = VR_CHECK_INPUT>
        addr_const_t r_advance (size_type const step)
        {
            DLOG_trace2 << "[size: " << size () << ", capacity: " << capacity () << "] r_advance (" << step << ')';

            // TODO prefetch/discard/flush logic

            if (CHECK_BOUNDS) check_within_inclusive (step, size ());

            return (m_r_addr = addr_plus (m_r_addr, step));
        }

        template<bool CHECK_BOUNDS = VR_CHECK_INPUT>
        addr_t w_advance (size_type const step) // TODO signature, do we need to return new data addr?
        {
            DLOG_trace2 << "[size: " << size () << ", capacity: " << capacity () << "] w_advance (" << step << ')';

            // TODO prefetch/discard/flush logic

            if (CHECK_BOUNDS) check_within_inclusive (step, capacity () - size ());

            return (m_w_addr = addr_plus (m_w_addr, step));
        }

    private: // ..............................................................

        void truncate_and_close () VR_NOEXCEPT_IF (VR_RELEASE); // idempotent // TODO allow this to be public?


        pos_t const m_reserve_size; // note: integral number of pages
        addr_t m_base { };
        addr_const_t m_r_addr { };
        addr_t m_w_addr { };
        int32_t m_fd { -1 };

}; // end of class

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
