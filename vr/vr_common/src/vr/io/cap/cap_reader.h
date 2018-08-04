#pragma once

#include "vr/io/cap/defs.h"
#include "vr/io/exceptions.h"
#include "vr/io/mapped_ring_buffer.h"
#include "vr/io/net/defs.h" // _packet_index_
#include "vr/io/pcap/pcaprec_hdr.h"
#include "vr/io/streams.h"
#include "vr/meta/integer.h"
#include "vr/util/logging.h"

#include <net/ethernet.h>
#include <netinet/ip.h>

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
/**
 * in this "universal" reader, a "record" means a successful data consumption visit by 'visitor'
 *
 * @see pcap_reader
 */
class cap_reader final: noncopyable
{
    public: // ...............................................................

        VR_ASSUME_COLD cap_reader (std::istream & in, cap_format::enum_t const format);

        // MUTATORs:

        /**
         * populates 'CTX' fields, if defined:
         *
         *   net::_packet_index_
         *
         * @note 'record_limit' is a soft limit since internal I/O buffer window
         *       will be attempted to be drained before checking it
         *
         * @return actual count of records consumed/visited (could be over 'record_limit', see the note)
         */
        template<typename CTX, typename V>
        VR_ASSUME_HOT int64_t
        evaluate (CTX & ctx, V && visitor, int64_t const record_limit = std::numeric_limits<int64_t>::max ());

        // TODO expose filtered count as well

    private: // ..............................................................

        static constexpr mapped_ring_buffer::size_type initial_buf_capacity ()  { return (256 * 1024); }

        /*
         * only used for 'cap_format::pcap'
         */
        using pcap_hdr_type = pcaprec_hdr<util::subsecond_duration_ns, /* little endian */true>;

        /*
         * re-constructs 'm_buf' if capacity larger than 'initial_buf_capacity()' needed
         */
        VR_ASSUME_COLD void ensure_buf_capacity (mapped_ring_buffer::size_type const capacity);

        /*
         * only used for 'cap_format::pcap'
         */
        VR_ASSUME_COLD int32_t read_pcap_header (std::istream & in);


        std::istream & m_in; // [not owned]
        mapped_ring_buffer m_buf { initial_buf_capacity () }; // capacity increased to 'snaplen' at construction time if needed

}; // end of class
//............................................................................

template<typename CTX, typename V>
int64_t
cap_reader::evaluate (CTX & ctx, V && visitor, int64_t const record_limit)
{
    vr_static_assert (! std::is_const<CTX>::value); // design requirement

    int64_t record_index { };

    using size_type         = mapped_ring_buffer::size_type;

    static constexpr size_type min_available    = std::decay_t<V>::min_size (); // note: this is actually a static consexpr method of 'V'

    addr_const_t r = m_buf.r_position (); // invariant: points at the start of a "packet" (which may or may not have been read fully)
    addr_t w = m_buf.w_position (); // invariant: points at the next byte available for writing

    size_type available { };

    try
    {
        for (size_type r_count; (record_index < record_limit) && ((r_count = io::read_fully (m_in, m_buf.w_window (), w)) > 0); )
        {
            DLOG_trace3 << "  r_count = " << r_count;

            w = m_buf.w_advance (r_count);
            available += r_count;
            assert_positive (available);

            while (VR_LIKELY (available >= min_available))
            {
                if (has_field<net::_packet_index_, CTX> ())
                {
                    field<net::_packet_index_> (ctx) = record_index;
                }

                int32_t const vrc = visitor.consume (ctx, r, available); // note: this ('available') is where this reader differ from 'pcap_reader'

                if (VR_UNLIKELY (vrc <= 0))
                    break; // need to read more bytes into 'm_buf' before 'visitor' can consume another record

                // a record has been consumed:

                available -= vrc;
                DLOG_trace2 << "    [" << record_index << "]: record consumed " << vrc << " byte(s), available " << available << " byte(s)";

                r = m_buf.r_advance (vrc); // consumed

                ++ record_index;
            }
        }
    }
    catch (stop_iteration const &)
    {
        LOG_trace1 << "visitor requested an early stop";
    }

    LOG_info << "  read " << record_index << " record(s)";
    return record_index;
}

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
