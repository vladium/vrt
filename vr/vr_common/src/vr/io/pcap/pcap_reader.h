#pragma once

#include "vr/io/exceptions.h"
#include "vr/io/mapped_ring_buffer.h"
#include "vr/io/net/defs.h" // _packet_index_
#include "vr/io/pcap/pcaprec_hdr.h"
#include "vr/io/streams.h"
#include "vr/meta/integer.h"
#include "vr/util/logging.h"

#include <net/ethernet.h>
#include <netinet/ip.h>
#include <pcap/pcap.h>

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
//............................................................................
//............................................................................
namespace impl
{

// TODO for a 'mapped_ifile' input a different impl strategy (sans ring buffer) would be very efficient

class pcap_reader_base: noncopyable
{
    public: // ...............................................................

        using pcap_hdr_type = pcaprec_hdr<util::subsecond_duration_ns, /* little endian */true>;

        /*
         * made public as a utility method
         */
        static VR_ASSUME_COLD int32_t read_header (std::istream & in);

    protected: // ............................................................

        static constexpr mapped_ring_buffer::size_type initial_buf_capacity ()  { return (256 * 1024); }

        VR_ASSUME_COLD pcap_reader_base (std::istream & in);

        /*
         * re-constructs 'm_buf' if capacity larger than 'initial_buf_capacity()' needed
         */
        VR_ASSUME_COLD void ensure_buf_capacity (mapped_ring_buffer::size_type const capacity);


        std::istream & m_in; // [not owned]
        mapped_ring_buffer m_buf { initial_buf_capacity () }; // capacity increased to 'snaplen' at construction time if needed

}; // end of class

} // end of 'impl'
//............................................................................
//............................................................................
/**
 * TODO currently this checks for 'LE == true' encoding at runtime (read_header ())
 * but otherwise the API supports both types
 *
 * @note this assumes link-level (ethernet) capture
 *
 * @see cap_reader
 */
template<bool ASSUME_IP_ENCAPSULATION = true>
class pcap_reader: public impl::pcap_reader_base
{
    private: // ..............................................................

        using super         = impl::pcap_reader_base;

    public: // ...............................................................

        static constexpr int32_t read_offset ()             { return sizeof (pcap_hdr_type); }
        static constexpr bool assume_IP_encapsulation ()    { return ASSUME_IP_ENCAPSULATION; }

        VR_ASSUME_COLD pcap_reader (std::istream & in) :
            super (in)
        {
        }

        // MUTATORs:

        /**
         * populates 'CTX' fields, if defined:
         *
         *   net::_packet_index_
         *
         * @note 'packet_limit' is a soft limit since internal I/O buffer window
         *       will be attempted to be drained before checking it
         *
         * @return actual count of packets consumed/visited (could be over 'packet_limit', see the note)
         */
        template<typename V>
        VR_ASSUME_HOT int64_t
        evaluate (V && packet_visitor, int64_t const packet_limit = std::numeric_limits<int64_t>::max ());

}; // end of class
//............................................................................

template<bool ASSUME_IP_ENCAPSULATION>
template<typename V>
int64_t
pcap_reader<ASSUME_IP_ENCAPSULATION>::evaluate (V && packet_visitor, int64_t const packet_limit)
{
    using size_type         = typename mapped_ring_buffer::size_type;

    int64_t packet_index { };

    addr_const_t r = m_buf.r_position (); // invariant: points at the start of a capture header (which may or may not have been read fully)
    addr_t w = m_buf.w_position (); // invariant: points at the next byte available for writing

    size_type available { };

    try
    {
        for (size_type r_count; (packet_index < packet_limit) && ((r_count = read_fully (m_in, m_buf.w_window (), w)) > 0); )
        {
            DLOG_trace2 << "  r_count = " << r_count;

            w = m_buf.w_advance (r_count);
            available += r_count;

            while (VR_LIKELY (available >= read_offset ())) // ensure we can read up to the included length field
            {
                pcap_hdr_type const & pcap_hdr = * reinterpret_cast<pcap_hdr_type const *> (r);

                size_type const pcap_incl_len = pcap_hdr.incl_len ();
                size_type const pcap_size = read_offset () + pcap_incl_len;

                if (VR_UNLIKELY (available < pcap_size))
                    break; // need to read more bytes into 'm_buf'


                DLOG_trace1 << "    [" << packet_index << ", " << print_timestamp (pcap_hdr.get_timestamp ()) << "]: pcap incl/orig len: " << pcap_incl_len << '/' << pcap_hdr.orig_len ();

                assert_le (pcap_incl_len, static_cast<size_type> (pcap_hdr.orig_len ())); // data invariant
                assert_within (pcap_hdr.ts_fraction (), pcap_hdr_type::ts_fraction_limit ()); // data invariant

                ::ether_header const * const ether_hdr = reinterpret_cast<::ether_header const * > (addr_plus (r, read_offset ()));

                if (ASSUME_IP_ENCAPSULATION) // compile-time check
                {
                    assert_eq (ether_hdr->ether_type, meta::static_host_to_net<decltype (ether_hdr->ether_type)> (ETHERTYPE_IP));
                }

                if (! ASSUME_IP_ENCAPSULATION || VR_LIKELY (ether_hdr->ether_type == meta::static_host_to_net<decltype (ether_hdr->ether_type)> (ETHERTYPE_IP)))
                {
                    auto & ctx = const_cast<pcap_hdr_type &> (pcap_hdr); // this is ugly, but is needed to support templated CRTP-style derivations without too much pain

                    if (has_field<net::_packet_index_, pcap_hdr_type> ())
                    {
                        field<net::_packet_index_> (ctx) = packet_index;
                    }

                    // note: this (always consumption of 'pcap_incl_len') is where this reader differs from 'cap_reader':

                    VR_IF_DEBUG (int32_t const pvrc = )packet_visitor.consume (ctx, addr_plus (r, read_offset ()), pcap_incl_len);
                    assert_eq (pvrc, pcap_incl_len);
                }

                available -= pcap_size;
                r = m_buf.r_advance (pcap_size); // consumed

                ++ packet_index;
            }
        }
    }
    catch (stop_iteration const &)
    {
        LOG_trace1 << "visitor requested an early stop";
    }

    LOG_info << "  read " << packet_index << " packet(s)";
    return packet_index;
}

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
