#pragma once

#include "vr/arg_map.h"
#include "vr/fields.h"
#include "vr/io/net/defs.h" // _dst_port_
#include "vr/io/net/io.h" // print() override for ::udphdr
#include "vr/io/net/utility.h" // min_size_or_zero
#include "vr/util/datetime.h"
#include "vr/util/logging.h"
#include "vr/util/ops_int.h"

#include <netinet/udp.h>

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
namespace net
{
//............................................................................

class null_UDP_data_visitor
{
    public: // ...............................................................

        static constexpr int32_t min_size ()    { return 0; }

        null_UDP_data_visitor (arg_map const & args)
        {
        }


        template<typename CTX>
        VR_FORCEINLINE void
        visit_data (CTX & ctx, addr_const_t const data, int32_t const size)
        {
            // [no-op]
        }

}; // end of class
//............................................................................
/**
 */
template<typename ENCAPSULATED = null_UDP_data_visitor>
class UDP_: public ENCAPSULATED
{
    private: // ..............................................................

        using encapsulated  = ENCAPSULATED;

    public: // ...............................................................

        static constexpr int32_t proto ()       { return IPPROTO_UDP; }

        static constexpr int32_t read_offset () { return sizeof (::udphdr); }
        static constexpr int32_t min_size ()    { return (read_offset () + min_size_or_zero<ENCAPSULATED>::value ()); }


        using encapsulated::encapsulated; // inherit constructors

        /**
         * 'size' bytes of data is available starting at 'udp_hdr'
         *
         * populates 'CTX' fields, if defined:
         *
         *   net::_dst_port_
         *   net::_filtered_
         */
        template<typename CTX>
        VR_FORCEINLINE void
        visit_proto (CTX & ctx, ::udphdr const * const udp_hdr, int32_t const size)
        {
            assert_le (read_offset (), size);
            assert_eq (size, util::net_to_host (udp_hdr->len));

            if (has_field<net::_dst_port_, CTX> ())
            {
                field<net::_dst_port_> (ctx) = util::net_to_host (udp_hdr->dest);
            }

            if (has_field<_filtered_, CTX> ())
            {
                field<_filtered_> (ctx) = false;
            }

            VR_IF_DEBUG
            (
                using _ts_              = util::if_t<(has_field<_ts_local_, CTX> ()), _ts_local_, _ts_origin_>;

                constexpr bool have_ts              = has_field<_ts_, CTX> ();
                constexpr bool have_packet_index    = has_field<_packet_index_, CTX> ();

                switch ((have_packet_index << 1) | have_ts) // compile-time switch
                {
                    case 0b11: DLOG_trace2 << "\t[" << field<_packet_index_> (ctx) << ", " << print_timestamp (static_cast<timestamp_t> (field<_ts_> (ctx))) << "]: " << print (* udp_hdr); break;
                    case 0b10: DLOG_trace2 << "\t[" << field<_packet_index_> (ctx) << "]: " << print (* udp_hdr); break;
                    case 0b01: DLOG_trace2 << "\t[" << print_timestamp (static_cast<timestamp_t> (field<_ts_> (ctx))) << "]: " << print (* udp_hdr); break;
                    default:   DLOG_trace2 << '\t' << print (* udp_hdr); break;

                } // end of switch
            )

            encapsulated::visit_data (ctx, addr_plus (udp_hdr, read_offset ()), size - read_offset ());
        }

}; // end of class

} // end of 'net'
} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
