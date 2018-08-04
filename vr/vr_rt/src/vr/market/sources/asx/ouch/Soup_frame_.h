#pragma once

#include "vr/fields.h"
#include "vr/io/net/defs.h" // _packet_index_, _src/dst_port_
#include "vr/market/net/SoupTCP_.h"
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
 * extension of 'SoupTCP_' that overrides/extends only 'visit_SoupTCP_payload()'
 *
 * @note you only need to use non-void 'DERIVED' if you're overriding SoupBinTCP visits
 */
template<io::mode::enum_t IO_MODE, typename ENCAPSULATED, typename DERIVED = void> // a slight twist on CRTP
class Soup_frame_: public impl::seqnum_state, public SoupTCP_<IO_MODE, ENCAPSULATED, util::this_or_derived_t<Soup_frame_<IO_MODE, ENCAPSULATED, DERIVED>, DERIVED>>
{
    private: // ..............................................................

        using this_type     = Soup_frame_<IO_MODE, ENCAPSULATED, DERIVED>;
        using super         = SoupTCP_<IO_MODE, ENCAPSULATED, util::this_or_derived_t<this_type, DERIVED> >;

    public: // ...............................................................

        using super::super; // inherit constructors

        /*
         * TODO (copied from SoupTCP_) the only meaningful ts to display here is _ts_local_, but it isn't stored in a 'wire' format
         */

        // SoupTCP_:

        /**
         * override to descend into the payload message and deal with partition indices and seqnums
         */
        template<typename CTX>
        void
        visit_SoupTCP_payload (CTX & ctx, SoupTCP_packet_hdr const & packet_hdr, addr_const_t/* ouch::*message */const msg) // override
        {
            vr_static_assert (std::is_base_of<ENCAPSULATED, this_type>::value);

            // note: SoupTCP is documented to always wrap a single higher-level protocol message:

            using io::net::_packet_index_;
            using _ts_          = select_display_ts_t<CTX>;

            constexpr bool have_ts              = has_field<_ts_, CTX> ();
            constexpr bool have_packet_index    = has_field<_packet_index_, CTX> ();
            constexpr bool have_partition       = has_field<_partition_, CTX> ();


            /*
             * validate seqnum sequencing (per-partition state is maintained in 'impl::seqnum_state'),
             * which for Soup is mostly our own code check since Soup is TCP-based:
             */
            if (have_partition) // can manage and log 'pix' and 'sn'
            {
                int32_t const & pix = field<_partition_> (ctx); // set elsewhere
                assert_within (pix, partition_count ());

                seqnum_t & sn = m_seqnum_expected [pix];
                seqnum_t const sn_expected = sn;

                sn = sn_expected + 1; // always set the next expectation

                switch ((have_packet_index << 1) | have_ts) // compile-time switch
                {
                    case 0b11: DLOG_trace1 << "\t[" << field<_packet_index_> (ctx) << ", P" << pix << ", sn " << sn_expected << " -> " << sn << ", " << print_timestamp (static_cast<timestamp_t> (field<_ts_> (ctx))) << "]: Soup message size " << packet_hdr.length (); break;
                    case 0b10: DLOG_trace1 << "\t[" << field<_packet_index_> (ctx) << ", P" << pix << ", sn " << sn_expected << " -> " << sn << "]: Soup message size " << packet_hdr.length (); break;
                    case 0b01: DLOG_trace1 << "\t[P" << pix << ", sn " << sn_expected << " -> " << sn << ", " << print_timestamp (static_cast<timestamp_t> (field<_ts_> (ctx))) << "]: Soup message size " << packet_hdr.length (); break;
                    default:   DLOG_trace1 << "\t[P" << pix << ", sn " << sn_expected << " -> " << sn << "]: Soup message size " << packet_hdr.length (); break;

                } // end of switch
            }
            else
            {
                // TODO use a macro to re-use common parts:

                switch ((have_packet_index << 1) | have_ts) // compile-time switch
                {
                    case 0b11: DLOG_trace1 << "\t[" << field<_packet_index_> (ctx) << ", " << print_timestamp (static_cast<timestamp_t> (field<_ts_> (ctx))) << "]: Soup message size " << packet_hdr.length (); break;
                    case 0b10: DLOG_trace1 << "\t[" << field<_packet_index_> (ctx) << "]: Soup message size " << packet_hdr.length (); break;
                    case 0b01: DLOG_trace1 << "\t[" << print_timestamp (static_cast<timestamp_t> (field<_ts_> (ctx))) << "]: Soup message size " << packet_hdr.length (); break;
                    default:   DLOG_trace1 << "\tSoup message size " << packet_hdr.length (); break;

                } // end of switch
            }

            /*
             * visit single encapsulated message:
             */
            ENCAPSULATED::_internal_message_visit (msg, ctx);
        }

}; // end of class

} // end of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
