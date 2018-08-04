#pragma once

#include "vr/arg_map.h"
#include "vr/fields.h"
#include "vr/market/net/defs.h"
#include "vr/meta/structs.h"
#include "vr/util/datetime.h"
#include "vr/util/logging.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
//............................................................................

VR_META_PACKED_STRUCT (MoldUDP64_rewind_request,

    (alphanum_ft<10>,   session)
    (mold_seqnum_ft,    seqnum)
    (be_int16_ft,       msg_count)

); // end of class
//............................................................................

VR_META_PACKED_STRUCT (MoldUDP64_recv_packet_hdr,

    (alphanum_ft<10>,   session)
    (mold_seqnum_ft,    seqnum)
    (be_int16_ft,       msg_count)

); // end of class

VR_META_PACKED_STRUCT (MoldUDP64_recv_message_hdr,

    (be_int16_ft,       length)

); // end of class
//............................................................................
/**
 * a base MoldUDP64 implementation of 'visit_data()' that splits it into
 * three overridable visits:
 *
 * @code
 *  visit_MoldUDP64_payload()
 *  visit_MoldUDP64_heartbeat()
 *  visit_MoldUDP64_eos()
 * @endcode
 *
 * @note you need to use a non-void 'DERIVED' to be able to override these
 *       MoldUDP64 visits higher up the stack
 */
template<typename ENCAPSULATED, typename DERIVED = void> // a slight twist on CRTP
class MoldUDP64_: public ENCAPSULATED
{
    private: // ..............................................................

        using this_type     = MoldUDP64_<ENCAPSULATED, DERIVED>;
        using derived       = util::if_is_void_t<DERIVED, this_type, DERIVED>; // a slight twist on CRTP

        using encapsulated  = ENCAPSULATED;

    public: // ...............................................................

        static constexpr int32_t hdr_len ()     { return sizeof (MoldUDP64_recv_packet_hdr); }
        static constexpr int32_t min_size ()    { return hdr_len (); } // not strictly necessary at this layer level


        using encapsulated::encapsulated; // inherit constructors


        template<typename CTX>
        VR_FORCEINLINE int32_t
        visit_data (CTX & ctx, addr_const_t/* MoldUDP64_recv_packet_hdr */const data, int32_t const size)
        {
            vr_static_assert (std::is_base_of<this_type, derived>::value);

            assert_le (hdr_len (), size); // caller guarantee: data is presented in UDP datagrams

            MoldUDP64_recv_packet_hdr const & packet_hdr = * static_cast<MoldUDP64_recv_packet_hdr const *> (data);

            int32_t const msg_count = packet_hdr.msg_count ();

            // heartbeats don't advance seqnums, so split their handling off:

            if (VR_LIKELY (vr_is_in_exclusive_range (msg_count, 0, 0xFFFF))) // single-branch check
                static_cast<derived *> (this)->visit_MoldUDP64_payload (ctx, packet_hdr, msg_count, addr_plus (data, hdr_len ()));
            else
            {
                if (VR_LIKELY (! msg_count)) // heartbeat:
                    static_cast<derived *> (this)->visit_MoldUDP64_heartbeat (ctx, packet_hdr);
                else // EOS:
                {
                    assert_eq (msg_count, 0xFFFF);
                    static_cast<derived *> (this)->visit_MoldUDP64_eos (ctx, packet_hdr);
                }
            }

            return size; // entire packet is consumed
        }

        // overridable visits:

        template<typename CTX>
        VR_FORCEINLINE void
        visit_MoldUDP64_payload (CTX & ctx, MoldUDP64_recv_packet_hdr const & packet_hdr, int32_t const msg_count, addr_const_t/* packet */const data)
        {
            // [no-op: subclasses can override]

            VR_IF_DEBUG
            (
                using _ts_          = select_display_ts_t<CTX>;
                constexpr bool use_ts_local_delta = (std::is_same<_ts_local_, _ts_>::value && has_field<io::_ts_local_delta_, CTX> ());

                timestamp_t const ts_delta { use_ts_local_delta ? field<io::_ts_local_delta_> (ctx) : 0 };

                if (use_ts_local_delta) // compile-time branch
                    DLOG_trace2 << '[' << print_timestamp (field<_ts_> (ctx)) << " (+" << ts_delta << " ns)]: " << packet_hdr;
                else
                    DLOG_trace2 << '[' << print_timestamp (field<_ts_> (ctx)) << "]: " << packet_hdr;
            )
        }

        template<typename CTX>
        void
        visit_MoldUDP64_heartbeat (CTX & ctx, MoldUDP64_recv_packet_hdr const & packet_hdr)
        {
            // [no-op: subclasses can override]

            using _ts_          = select_display_ts_t<CTX>;
            constexpr bool use_ts_local_delta = (std::is_same<_ts_local_, _ts_>::value && has_field<io::_ts_local_delta_, CTX> ());

            timestamp_t const ts_delta { use_ts_local_delta ? field<io::_ts_local_delta_> (ctx) : 0 };

            if (use_ts_local_delta) // compile-time branch
                DLOG_trace3 << "HEARTBEAT: [" << print_timestamp (field<_ts_> (ctx)) << " (+" << ts_delta << " ns)]";
            else
                DLOG_trace3 << "HEARTBEAT: [" << print_timestamp (field<_ts_> (ctx)) << ']';
        }

        template<typename CTX>
        void
        visit_MoldUDP64_eos (CTX & ctx, MoldUDP64_recv_packet_hdr const & packet_hdr)
        {
            // [no-op: subclasses can override]

            using _ts_          = select_display_ts_t<CTX>;

            DLOG_trace1 << "EOS: [" << print_timestamp (field<_ts_> (ctx)) << ']';
        }

}; // end of class

} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
