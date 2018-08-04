#pragma once

#include "vr/io/pcap/pcaprec_hdr.h"
#include "vr/util/logging.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
namespace net // TODO move to 'io'?
{
/**
 * populates 'CTX' fields, if defined:
 *
 *   _ts_local_
 *   _ts_local_delta_
 */
template<typename ENCAPSULATED>
class pcap_: public ENCAPSULATED
{
    private: // ..............................................................

        using encapsulated  = ENCAPSULATED;

    public: // ...............................................................

        using pcap_hdr_type = pcaprec_hdr<util::subsecond_duration_ns, /* little endian */true>;

        static constexpr int32_t read_offset () { return sizeof (pcap_hdr_type); }
        static constexpr int32_t min_size ()    { return (ENCAPSULATED::min_size () + read_offset ()); }


        using encapsulated::encapsulated; // inherit constructors

        /**
         * 'available' bytes of data are available starting at 'data' (there's a trivial comment)
         *
         * populates 'CTX' fields, if defined:
         *
         *   _ts_local_
         *   _ts_local_delta_
         */
        template<typename CTX>
        VR_FORCEINLINE int32_t
        consume (CTX & ctx, addr_const_t const data, int32_t const available)
        {
            assert_ge (available, min_size ()); // caller guarantee

            // [it is safe to read pcap header]

            pcap_hdr_type const & pcap_hdr = * static_cast<pcap_hdr_type const *> (data);

            uint32_t const pcap_incl_len = pcap_hdr.incl_len ();

            constexpr bool have_packet_index    = has_field<_packet_index_, CTX> ();

            {
                if (have_packet_index) // compile-time branch
                    DLOG_trace1 << "    [" << field<_packet_index_> (ctx) << ", " << print_timestamp (pcap_hdr.get_timestamp ()) << "]: pcap incl/orig len: " << pcap_incl_len << '/' << pcap_hdr.orig_len ();
                else
                    DLOG_trace1 << "    [" << print_timestamp (pcap_hdr.get_timestamp ()) << "]: pcap incl/orig len: " << pcap_incl_len << '/' << pcap_hdr.orig_len ();

                assert_le (pcap_incl_len, static_cast<uint32_t> (pcap_hdr.orig_len ())); // data invariant
                assert_within (pcap_hdr.ts_fraction (), pcap_hdr_type::ts_fraction_limit ()); // data invariant
            }

            timestamp_t ts_local { }; // next value of 'm_ts_local_prev' if the current record commits

            if (has_field<_ts_local_, CTX> () || has_field<_ts_local_delta_, CTX> ())
            {
                ts_local = pcap_hdr.get_timestamp (); // propagate pcap capture timestamp as _ts_local_

                if (has_field<_ts_local_, CTX> ())
                {
                    field<_ts_local_> (ctx) = ts_local;
                }
                if (has_field<_ts_local_delta_, CTX> ())
                {
                    field<_ts_local_delta_> (ctx) = ts_local - m_ts_local_prev;
                }
            }

            int32_t const erc = ENCAPSULATED::consume (ctx, addr_plus (data, read_offset ()), available - read_offset ());
            {
                if (have_packet_index) // compile-time branch
                    DLOG_trace1 << "    [" << field<_packet_index_> (ctx) << ", " << print_timestamp (pcap_hdr.get_timestamp ()) << "]: erc = " << erc;
                else
                    DLOG_trace1 << "    [" << print_timestamp (pcap_hdr.get_timestamp ()) << "]: erc = " << erc;
            }

            if (VR_UNLIKELY (erc < signed_cast (pcap_incl_len))) // entire capture packet
                return -1;

            // commit current record:

            if (has_field<_ts_local_delta_, CTX> ())
            {
                m_ts_local_prev = ts_local;
            }

            return (erc + read_offset ()); // total consumed at this layer
        }

    private: // ..............................................................

        timestamp_t m_ts_local_prev { };

}; // end of class

} // end of 'net'
} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
