#pragma once

#include "vr/fields.h"
#include "vr/io/net/defs.h" // _packet_index_, _src/dst_port_
#include "vr/market/net/MoldUDP64_.h"
#include "vr/market/sources/asx/defs.h" // _partition_, partition_count(), impl::seqnum_state
#include "vr/util/logging.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
namespace ASX
{
/**
 * extension of 'MoldUDP64_' that overrides only 'visit_MoldUDP64_payload()'
 * and adds seqnum checking logic
 *
 * @note you only need to use non-void 'DERIVED' if you're overriding MoldUDP64 visits
 */
template<typename ENCAPSULATED, typename DERIVED = void> // a slight twist on CRTP
class Mold_frame_: public impl::seqnum_state, public MoldUDP64_<ENCAPSULATED, util::if_is_void_t<DERIVED, Mold_frame_<ENCAPSULATED>, DERIVED> > // a slight twist on CRTP
{
    private: // ..............................................................

        using this_type     = Mold_frame_<ENCAPSULATED, DERIVED>;
        using super         = MoldUDP64_<ENCAPSULATED, util::if_is_void_t<DERIVED, this_type, DERIVED> >;

    public: // ...............................................................

        using super::super; // inherit constructors


        // MoldUDP64_:

        /**
         * override to descend into payload message block and deal with partition indices and seqnums
         */
        template<typename CTX>
        VR_FORCEINLINE void
        visit_MoldUDP64_payload (CTX & ctx, MoldUDP64_recv_packet_hdr const & packet_hdr, int32_t const msg_count, addr_const_t/* MoldUDP64_recv_message_hdr */data) // override
        {
            vr_static_assert (std::is_base_of<ENCAPSULATED, this_type>::value);

            assert_positive (msg_count); // design invariant

            using _ts_          = select_display_ts_t<CTX>;

            /*
             * validate seqnum sequencings (per-partition state is maintained in 'impl::seqnum_state');
             *
             * also:
             *
             *  - set _partition_ if 'CTX' has _dst_port_, otherwise assume it's already set by the caller
             *  - set _seqnum_ if 'CTX' has it (based on data in 'packet_hdr')
             */
            if (has_field<io::net::_dst_port_, CTX> () | has_field<_partition_, CTX> ())
            {
                int32_t pix;

                if (has_field<io::net::_dst_port_, CTX> ()) // infer from destination UDP port
                {
                    pix = field<io::net::_dst_port_> (ctx) - partition_port_base ('A'); // TODO hardcoded feed choice

                    if (has_field<_partition_, CTX> ())
                    {
                        field<_partition_> (ctx) = pix;
                    }
                }
                else // if (has_field<_partition_, CTX> ())
                {
                    pix = field<_partition_> (ctx);
                }
                assert_within (pix, partition_count ());


                int64_t const sn_incoming = packet_hdr.seqnum ();
                assert_positive (sn_incoming);

                if (has_field<_seqnum_, CTX> ())
                {
                    field<_seqnum_> (ctx) = sn_incoming;
                }

                seqnum_t & sn = m_seqnum_expected [pix];
                seqnum_t const sn_expected = sn;

                sn = sn_incoming + msg_count; // always set the next expectation

                if (VR_UNLIKELY (sn_incoming != sn_expected))
                {
                    if (VR_LIKELY (sn_expected != 0)) // initial condition guard
                    {
                        LOG_warn << "[P" << pix << ", " << print_timestamp (field<_ts_> (ctx)) << "] seqnum GAP (" << (sn_incoming - sn_expected) << "): " << sn_incoming << ", expected " << sn_expected;
                    }
                }

                DLOG_trace1 << "  [P" << pix << ", sn " << sn_expected << " -> " << sn << ", " << print_timestamp (field<_ts_> (ctx)) << "] Mold msg count " << msg_count;
            }
            else
            {
                DLOG_trace1 << "  [" << print_timestamp (field<_ts_> (ctx)) << "] Mold msg count " << msg_count;
            }

            /*
             * visit each "message [block]" in the packet (prefixed by 'MoldUDP64_recv_message_hdr')
             *
             * note: it seems like MoldUDP "message length" (MoldUDP64_recv_message_hdr::length) is redundant
             *       wrt to the ITCH message type it contains, especially if each "message [block]" actually always
             *       contains a single ITCH message -- we can thus advance through the loop based on ITCH message
             *       types, without a memload from 'MoldUDP64_recv_message_hdr::length';
             *
             * later: experience with stacks of visitors relying on deep inheritance shows that it's awkward
             *        to have to keep passing the return codes through, so changed the design to use the MoldUDP
             *        header length after all:
             */
            ENCAPSULATED::_internal_packet_mark (pre_packet { msg_count }, ctx);
            {
                for (int32_t m = 0; m < msg_count; ++ m)
                {
                    MoldUDP64_recv_message_hdr const & msg_hdr = * static_cast<MoldUDP64_recv_message_hdr const *> (data);

                    data = addr_plus (data, sizeof (MoldUDP64_recv_message_hdr));

                    ENCAPSULATED::_internal_message_visit (data, ctx);

                    data = addr_plus (data, msg_hdr.length ());
                }
            }
            ENCAPSULATED::_internal_packet_mark (post_packet { msg_count }, ctx);
        }

}; // end of class

} // end of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
