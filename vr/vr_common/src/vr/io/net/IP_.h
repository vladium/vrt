#pragma once

#include "vr/arg_map.h"
#include "vr/fields.h"
#include "vr/io/defs.h" // _ts_local_, _ts_origin_
#include "vr/io/net/filters.h"
#include "vr/io/net/io.h" // print() override for ::ip
#include "vr/io/net/utility.h" // max_min_size
#include "vr/util/datetime.h" // print_timestamp
#include "vr/util/logging.h"

#include <net/ethernet.h>
#include <netinet/in.h> // IPPROTO_*
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
namespace net
{
/**
 * @note this is a "raw" implementation (assumes an eth header before the IP header)
 *
 * @note assumes IPv4
 */
template<typename ... ENCAPSULATED>
class IP_: public ENCAPSULATED ...,
           drop_packet_handler // leverage EBO if possible
{
    private: // ..............................................................

        using default_handler   = drop_packet_handler;

        using TCP_handler       = typename select_by_proto<IPPROTO_TCP,  ENCAPSULATED ...>::type;
        using UDP_handler       = typename select_by_proto<IPPROTO_UDP,  ENCAPSULATED ...>::type;

        static constexpr int32_t min_payload ()     { return 46; } // min payload size without 802.1Q tag present

    public: // ...............................................................

        static constexpr int32_t eth_hdr_len ()     { return sizeof (::ether_header); }

        static constexpr int32_t min_read_offset () { return (eth_hdr_len () + sizeof (::ip)); } // raw
        static constexpr int32_t min_size ()        { return (min_read_offset () + min_min_size<default_handler, ENCAPSULATED ...>::value ()); }

        IP_ (arg_map const & args) :
            ENCAPSULATED { args } ...
        {
        }


        /**
         * @see SoupTCP_
         */
        template<typename CTX>
        VR_FORCEINLINE int32_t
        consume (CTX & ctx, addr_const_t/* ::ether_header */const data, int32_t const available)
        {
            assert_ge (available, min_size ()); // caller guarantee

            // [it is safe to read ethernet and IP headers]

            VR_IF_DEBUG // assume IP encapsulation:
            (
                ::ether_header const * const ether_hdr = static_cast<::ether_header const * > (data);

                assert_eq (ether_hdr->ether_type, meta::static_host_to_net<decltype (ether_hdr->ether_type)> (ETHERTYPE_IP));
            )

            ::ip const * const ip_hdr = static_cast<::ip const *> (addr_plus (data, eth_hdr_len ()));

            int32_t const ip_len            = util::net_to_host (ip_hdr->ip_len);
            int32_t const raw_len           = (eth_hdr_len () + std::max<int32_t> (min_payload (), ip_len)); // raw, possibly padded

            // TODO the following check is a redundant branch in case of actual IP multicast socket data:

            if (VR_UNLIKELY (available < raw_len)) // partial packet read
            {
                DLOG_trace1 << "partial IP packet read: available (" << available << ") < raw_len (" << raw_len << "): " << print (* ip_hdr);
                return -1;
            }

            VR_IF_DEBUG
            (
                using _ts_          = util::if_t<(has_field<_ts_local_, CTX> ()), _ts_local_, _ts_origin_>;

                constexpr bool have_ts              = has_field<_ts_, CTX> ();
                constexpr bool have_packet_index    = has_field<_packet_index_, CTX> ();

                switch ((have_packet_index << 1) | have_ts) // compile-time switch
                {
                    case 0b11: DLOG_trace2 << "\t[" << field<_packet_index_> (ctx) << ", " << print_timestamp (static_cast<timestamp_t> (field<_ts_> (ctx))) << "]: " << print (* ip_hdr); break;
                    case 0b10: DLOG_trace2 << "\t[" << field<_packet_index_> (ctx) << "]: " << print (* ip_hdr); break;
                    case 0b01: DLOG_trace2 << "\t[" << print_timestamp (static_cast<timestamp_t> (field<_ts_> (ctx))) << "]: " << print (* ip_hdr); break;
                    default:   DLOG_trace2 << '\t' << print (* ip_hdr); break;

                } // end of switch
            )

            // handle IP headers with options:
            // [actual header may be longer than 'min_read_offset ()']

            int32_t const ip_hl             = ((ip_hdr->ip_hl) << 2);

            int32_t const proto_size        = (ip_len - ip_hl);
            addr_const_t const proto_data   = addr_plus (ip_hdr, ip_hl);

            switch (ip_hdr->ip_p) // TODO verify dead code asm elimination below
            {
                case IPPROTO_TCP:
                {
                    impl::dispatch<TCP_handler, default_handler>::evaluate (this, ctx, static_cast<::tcphdr const *> (proto_data), proto_size);
                }
                break;

                case IPPROTO_UDP:
                {
                    impl::dispatch<UDP_handler, default_handler>::evaluate (this, ctx, static_cast<::udphdr const *> (proto_data), proto_size);
                }
                break;

                default:
                {
                    static_cast<default_handler *> (this)->mark_filtered (ctx, ip_hdr, available - eth_hdr_len ()); // note: for logging purposes this doesn't advance to proto hdr
                }
                break;

            } // end of switch

            return raw_len; // total consumed at this layer;
        }

    private: // ..............................................................

        template<typename HANDLER, typename HANDLER_FALLBACK>
        friend class impl::dispatch; // grant access to inherited 'drop_packet_handler'

}; // end of class

} // end of 'net'
} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
