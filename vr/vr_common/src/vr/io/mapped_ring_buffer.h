#pragma once

#include "vr/asserts.h"
#include "vr/arg_map.h"
#include "vr/io/defs.h"
#include "vr/util/logging.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
// TODO constr option for pre-faulting (MAP_POPULATE)
// TODO constr option for mapping a huge page
/**
 * a buffer implementation that uses the "modulo-mmap" trick to provide
 * a finite-capacity ring buffer for variable-length records that "wraps around"
 * in virtual memory space such that no record (of size <= 'capacity') ever
 * needs to be read/written in two chunks that are split by a buffer boundary;
 *
 * this buffer does not do any mmap op, heap allocation, or page faulting after
 * construction and is thus very suitable for low-latency I/O;
 *
 *  - capacity: finite, fixed at construction time;
 *  - size: dynamic in [0, capacity] range, i.e. the count of bytes currently in the buffer
 *          (the unconsumed bytes between the [conceptual] write- and read-positions);
 *  - w_window: dynamic in [0, capacity] range, i.e. the count of unused capacity bytes;
 *
 *  - invariant(s): size + w_window == capacity;
 *
 *  - data stream persistent: no
 */
class mapped_ring_buffer // movable, but not assignable/copyable
{
    public: // ...............................................................

        using size_type             = pos_t;

        /**
         * @param capacity minimum capacity requested [actual capacity will be rounded up to a whole number of pages]
         */
        mapped_ring_buffer (size_type const capacity);
        mapped_ring_buffer (arg_map const & args);

        ~mapped_ring_buffer () VR_NOEXCEPT;

        mapped_ring_buffer (mapped_ring_buffer && rhs);

        // ACCESSORs:

        addr_const_t r_position () const
        {
             return (m_base + (static_cast<u_size_type> (m_r_position) & m_capacity_mask));
        }

        /**
         * @return count of bytes available for reading from current @ref r_position()
         *
         * @invariant size() + w_window() == capacity()
         */
        VR_FORCEINLINE size_type size () const
        {
            assert_within_inclusive (m_w_position - m_r_position, capacity ());

            return (m_w_position - m_r_position);
        }

        /**
         * @invariant size() + w_window() == capacity()
         */
        VR_FORCEINLINE size_type capacity () const
        {
            return (m_capacity_mask + 1);
        }

        /**
         * @return count of bytes mapped and available for writing from current @ref w_position()
         *
         * @invariant size() + w_window() == capacity()
         */
        VR_FORCEINLINE size_type w_window () const
        {
            // checked by 'size()': assert_within_inclusive (m_w_position - m_r_position, capacity ());

            return (capacity () - size ());
        }

        // MUTATORs:

        addr_t w_position ()
        {
            return (m_base + (static_cast<u_size_type> (m_w_position) & m_capacity_mask));
        }

        template<bool CHECK_BOUNDS = VR_CHECK_INPUT>
        addr_const_t r_advance (size_type const step) // TODO signature, do we need to return new data addr?
        {
            DLOG_trace2 << "[size: " << size () << ", capacity: " << capacity () << "] r_advance (" << step << ')';

            pos_t const r_position_new = m_r_position + step;
            if (CHECK_BOUNDS) check_in_inclusive_range (r_position_new, m_r_position, m_w_position);

            m_r_position = r_position_new;
            return (m_base + (static_cast<u_size_type> (r_position_new) & m_capacity_mask));
        }

        template<bool CHECK_BOUNDS = VR_CHECK_INPUT>
        addr_t w_advance (size_type const step) // TODO signature, do we need to return new data addr?
        {
            DLOG_trace2 << "[size: " << size () << ", capacity: " << capacity () << "] w_advance (" << step << ')';

            pos_t const w_position_new = m_w_position + step;
            if (CHECK_BOUNDS) check_in_inclusive_range (w_position_new, m_w_position, m_r_position + capacity ());

            m_w_position = w_position_new;
            return (m_base + (static_cast<u_size_type> (w_position_new) & m_capacity_mask));
        }

    private: // ..............................................................

        using u_size_type   = std::make_unsigned<size_type>::type;


        size_type const m_capacity_mask; // TODO may want to keep log2/mask of this instead
        int8_t * m_base { };
        pos_t m_r_position { };
        pos_t m_w_position { };

}; // end of class

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
