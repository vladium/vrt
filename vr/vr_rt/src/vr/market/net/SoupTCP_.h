#pragma once

#include "vr/arg_map.h"
#include "vr/fields.h"
#include "vr/io/net/defs.h" // _packet_index_
#include "vr/io/net/utility.h" // min_size_or_zero
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
// duplex (client <-> server):
//............................................................................

VR_META_PACKED_STRUCT (SoupTCP_packet_hdr,

    (be_int16_ft,       length)
    (char,              type)

); // end of class
//............................................................................
// client -> server:
//............................................................................

// 'L'
VR_META_PACKED_STRUCT (SoupTCP_login_request,

    (alphanum_ft<6>,    username)   // case-insensitive, right-padded with spaces
    (alphanum_ft<10>,   password)   // case-insensitive, right-padded with spaces
    (alphanum_ft<10>,   session)    // all-blanks means "currently active"
    (alphanum_ft<20>,   seqnum)     // NOTE: integer in ASCII

); // end of class
//............................................................................
// server -> client:
//............................................................................

// 'A'
VR_META_PACKED_STRUCT (SoupTCP_login_accepted,

    (alphanum_ft<10>,   session)    // right-padded with spaces
    (alphanum_ft<20>,   seqnum)     // ASCII seqnum of the next 'SoupTCP_sequenced_data', right-padded with spaces

); // end of class

// 'J'
VR_META_PACKED_STRUCT (SoupTCP_login_rejected,

    (alphanum_ft<10>,   reject_code)

); // end of class
//............................................................................
/**
 * a base SoupBinTCP implementation of 'visit_data()' that splits it into
 * several overridable visits (depending on 'IO_MODE'):
 *
 * @code
 *  visit_SoupTCP_payload()     // all 'S' sequenced packets
 *  visit_SoupTCP_heartbeat()
 *  visit_SoupTCP_eos()
 *  visit_SoupTCP_login()       // overloaded for "accepted" and "rejected" cases
 *  visit_SoupTCP_debug()
 * @endcode
 *
 * @note you need to use a non-void 'DERIVED' to be able to override these
 *       SoupBinTCP visits higher up the stack
 */
template<io::mode::enum_t IO_MODE, typename ENCAPSULATED, typename DERIVED = void> // a slight twist on CRTP
class SoupTCP_; // master

//............................................................................

template<typename ENCAPSULATED, typename DERIVED> // 'recv' specialization
class SoupTCP_<io::mode::recv, ENCAPSULATED, DERIVED>: public ENCAPSULATED
{
    private: // ..............................................................

        static constexpr io::mode::enum_t mode ()   { return io::mode::recv; }

        using this_type     = SoupTCP_<mode (), ENCAPSULATED, DERIVED>;
        using derived       = util::if_is_void_t<DERIVED, this_type, DERIVED>; // a slight twist on CRTP

        using encapsulated  = ENCAPSULATED;

    public: // ...............................................................

        static constexpr int32_t hdr_len ()     { return sizeof (SoupTCP_packet_hdr); }
        static constexpr int32_t min_size ()    { return (hdr_len () + io::net::min_size_or_zero<ENCAPSULATED>::value ()); }


        using encapsulated::encapsulated; // inherit constructors


        /*
         * TODO unless I add an equivalent of ITCH_pipeline, the 'bool' returns of
         * message visits aren't currently for any kind of "filtering" except by 'pre_message' hooks
         */
        /**
         * @note this class and its derivatives support zero-copy framing for TCP
         *       similar to what @ref IP_ does for IP protocols
         */
        template<typename CTX>
        VR_FORCEINLINE int32_t
        consume (CTX & ctx, addr_const_t/* SoupTCP_packet_hdr */const data, int32_t const available)
        {
            vr_static_assert (std::is_base_of<this_type, derived>::value);

            {
                using io::net::_packet_index_;
                using _ts_          = select_display_ts_t<CTX>;

                constexpr bool have_ts              = has_field<_ts_, CTX> ();
                constexpr bool have_packet_index    = has_field<_packet_index_, CTX> ();

                switch ((have_packet_index << 1) | have_ts) // compile-time switch
                {
                    case 0b11: DLOG_trace1 << "\t[" << field<_packet_index_> (ctx) << ", " << print_timestamp (static_cast<timestamp_t> (field<_ts_> (ctx))) << "]: available " << available; break;
                    case 0b10: DLOG_trace1 << "\t[" << field<_packet_index_> (ctx) << "]: available " << available; break;
                    case 0b01: DLOG_trace1 << "\t[" << print_timestamp (static_cast<timestamp_t> (field<_ts_> (ctx))) << "]: available " << available; break;
                    default:   DLOG_trace1 << "\available " << available; break;

                } // end of switch
            }

            assert_ge (available, min_size ()); // caller guarantee

            SoupTCP_packet_hdr const & packet_hdr = * static_cast<SoupTCP_packet_hdr const *> (data);

            int32_t const soup_len = hdr_len () + packet_hdr.length () -/* ! */1;

            if (VR_UNLIKELY (available < soup_len))
                return -1; // partial SoupTCP message

            int32_t const msg_type = packet_hdr.type ();

            if (VR_LIKELY (msg_type == 'S')) // frequent case
                static_cast<derived *> (this)->visit_SoupTCP_payload (ctx, packet_hdr, addr_plus (data, hdr_len ()));
            else
            {
                switch (msg_type)
                {
#               define vr_DISPATCH(MSG) \
                        static_cast<derived *> (this)->visit_SoupTCP_login (ctx, packet_hdr, * static_cast< MSG const *> (addr_plus (data, hdr_len ()))) \
                        /* */

                    case 'A': vr_DISPATCH (SoupTCP_login_accepted); break;
                    case 'J': vr_DISPATCH (SoupTCP_login_rejected); break;

#               undef vr_DISPATCH

                    case 'H': static_cast<derived *> (this)->visit_SoupTCP_heartbeat (ctx, packet_hdr); break;
                    case 'Z': static_cast<derived *> (this)->visit_SoupTCP_eos (ctx, packet_hdr); break;
                    case '+': static_cast<derived *> (this)->visit_SoupTCP_debug (ctx, packet_hdr); break;

                    default: VR_ASSUME_UNREACHABLE (soup_len, static_cast<int32_t> (packet_hdr.type ()));

                } // end of switch
            }

            return soup_len;
        }


        // overridable visits:

        template<typename CTX>
        void
        visit_SoupTCP_payload (CTX & ctx, SoupTCP_packet_hdr const & packet_hdr, addr_const_t/* packet */const data)
        {
            // [no-op: subclasses can override]

            VR_IF_DEBUG
            (
                using io::net::_packet_index_;
                using _ts_          = select_display_ts_t<CTX>;

                constexpr bool have_ts              = has_field<_ts_, CTX> ();
                constexpr bool have_packet_index    = has_field<_packet_index_, CTX> ();

                switch ((have_packet_index << 1) | have_ts) // compile-time switch
                {
                    case 0b11: DLOG_trace2 << "\t[" << field<_packet_index_> (ctx) << ", " << print_timestamp (static_cast<timestamp_t> (field<_ts_> (ctx))) << "]: " << packet_hdr; break;
                    case 0b10: DLOG_trace2 << "\t[" << field<_packet_index_> (ctx) << "]: " << packet_hdr; break;
                    case 0b01: DLOG_trace2 << "\t[" << print_timestamp (static_cast<timestamp_t> (field<_ts_> (ctx))) << "]: " << packet_hdr; break;
                    default:   DLOG_trace2 << '\t' << packet_hdr; break;

                } // end of switch
            )
        }


        template<typename CTX>
        void
        visit_SoupTCP_heartbeat (CTX & ctx, SoupTCP_packet_hdr const & packet_hdr)
        {
            // [no-op: subclasses can override]

            using _ts_          = select_display_ts_t<CTX>;

            vr_static_assert (has_field<_ts_, CTX> ());

            constexpr bool use_ts_local_delta   = (std::is_same<_ts_local_, _ts_>::value && has_field<io::_ts_local_delta_, CTX> ());

            timestamp_t const ts_delta { use_ts_local_delta ? field<io::_ts_local_delta_> (ctx) : 0 };

            if (use_ts_local_delta) // compile-time branch
                DLOG_trace3 << "HEARTBEAT: [" << print_timestamp (field<_ts_> (ctx)) << " (+" << ts_delta << " ns)]";
            else
                DLOG_trace3 << "HEARTBEAT: [" << print_timestamp (field<_ts_> (ctx)) << ']';
        }

        template<typename CTX>
        void
        visit_SoupTCP_eos (CTX & ctx, SoupTCP_packet_hdr const & packet_hdr)
        {
            // [no-op: subclasses can override]

            using _ts_          = select_display_ts_t<CTX>;

            vr_static_assert (has_field<_ts_, CTX> ());

            DLOG_trace1 << "EOS: [" << print_timestamp (field<_ts_> (ctx)) << ']';
        }

        template<typename CTX>
        void
        visit_SoupTCP_debug (CTX & ctx, SoupTCP_packet_hdr const & packet_hdr)
        {
            // [no-op: subclasses can override]

            using _ts_          = select_display_ts_t<CTX>;

            vr_static_assert (has_field<_ts_, CTX> ());

            DLOG_trace2 << "DEBUG message [" << print_timestamp (field<_ts_> (ctx)) << ']';
        }


        template<typename CTX>
        void
        visit_SoupTCP_login (CTX & ctx, SoupTCP_packet_hdr const & packet_hdr, SoupTCP_login_accepted const & msg)
        {
            // [no-op: subclasses can override]

            using _ts_          = select_display_ts_t<CTX>;

            vr_static_assert (has_field<_ts_, CTX> ());

            DLOG_trace1 << "\t[" << print_timestamp (field<_ts_> (ctx)) << "]: login accepted: " << msg;
        }

        template<typename CTX>
        void
        visit_SoupTCP_login (CTX & ctx, SoupTCP_packet_hdr const & packet_hdr, SoupTCP_login_rejected const & msg)
        {
            // [no-op: subclasses can override]

            using _ts_          = select_display_ts_t<CTX>;

            vr_static_assert (has_field<_ts_, CTX> ());

            DLOG_trace1 << "\t[" << print_timestamp (field<_ts_> (ctx)) << "]: login REJECTED: " << msg;
        }

}; // end of class
//............................................................................

template<typename ENCAPSULATED, typename DERIVED> // 'recv' specialization
class SoupTCP_<io::mode::send, ENCAPSULATED, DERIVED>: public ENCAPSULATED
{
    private: // ..............................................................

        static constexpr io::mode::enum_t mode ()   { return io::mode::send; }

        using this_type     = SoupTCP_<mode (), ENCAPSULATED, DERIVED>;
        using derived       = util::if_is_void_t<DERIVED, this_type, DERIVED>; // a slight twist on CRTP

        using encapsulated  = ENCAPSULATED;

    public: // ...............................................................

        static constexpr int32_t hdr_len ()     { return sizeof (SoupTCP_packet_hdr); }
        static constexpr int32_t min_size ()    { return (hdr_len () + io::net::min_size_or_zero<ENCAPSULATED>::value ()); }


        using encapsulated::encapsulated; // inherit constructors


        /**
         * @note this class and its derivatives support zero-copy framing for TCP
         *       similar to what @ref IP_ does for IP protocols
         */
        template<typename CTX>
        VR_FORCEINLINE int32_t
        consume (CTX & ctx, addr_const_t/* SoupTCP_packet_hdr */const data, int32_t const available)
        {
            vr_static_assert (std::is_base_of<this_type, derived>::value);

            assert_ge (available, min_size ()); // caller guarantee

            SoupTCP_packet_hdr const & packet_hdr = * static_cast<SoupTCP_packet_hdr const *> (data);

            int32_t const soup_len = hdr_len () + packet_hdr.length () -/* ! */1;

            if (VR_UNLIKELY (available < soup_len))
                return -1; // partial SoupTCP message

            int32_t const msg_type = packet_hdr.type ();

            if (VR_LIKELY (msg_type == 'U')) // frequent case
                static_cast<derived *> (this)->visit_SoupTCP_payload (ctx, packet_hdr, addr_plus (data, hdr_len ()));
            else
            {
                switch (msg_type)
                {
                    case 'L': static_cast<derived *> (this)->visit_SoupTCP_login_request (ctx, packet_hdr, * static_cast<SoupTCP_login_request const *> (addr_plus (data, hdr_len ()))); break;

                    case 'R': static_cast<derived *> (this)->visit_SoupTCP_heartbeat (ctx, packet_hdr); break;

                    default: VR_ASSUME_UNREACHABLE (packet_hdr.type ());

                } // end of switch
            }

            return soup_len;
        }

        /*
         * TODO the only meaningful ts to display here is _ts_local_, but it isn't stored in a 'wire' format
         */

        // overridable visits:

        template<typename CTX>
        void
        visit_SoupTCP_payload (CTX & ctx, SoupTCP_packet_hdr const & packet_hdr, addr_const_t/* packet */const data)
        {
            // [no-op: subclasses can override]

            VR_IF_DEBUG
            (
                using io::net::_packet_index_;
                using _ts_          = select_display_ts_t<CTX>;

                constexpr bool have_ts              = has_field<_ts_, CTX> ();
                constexpr bool have_packet_index    = has_field<_packet_index_, CTX> ();

                switch ((have_packet_index << 1) | have_ts) // compile-time switch
                {
                    case 0b11: DLOG_trace2 << "\t[" << field<_packet_index_> (ctx) << ", " << print_timestamp (static_cast<timestamp_t> (field<_ts_> (ctx))) << "]: " << packet_hdr; break;
                    case 0b10: DLOG_trace2 << "\t[" << field<_packet_index_> (ctx) << "]: " << packet_hdr; break;
                    case 0b01: DLOG_trace2 << "\t[" << print_timestamp (static_cast<timestamp_t> (field<_ts_> (ctx))) << "]: " << packet_hdr; break;
                    default:   DLOG_trace2 << '\t' << packet_hdr; break;

                } // end of switch
            )
        }


        template<typename CTX>
        void
        visit_SoupTCP_heartbeat (CTX & ctx, SoupTCP_packet_hdr const & packet_hdr)
        {
            // [no-op: subclasses can override]

            using io::net::_packet_index_;
            using _ts_          = select_display_ts_t<CTX>;

            constexpr bool have_ts              = has_field<_ts_, CTX> ();
            constexpr bool have_packet_index    = has_field<_packet_index_, CTX> ();

            switch ((have_packet_index << 1) | have_ts) // compile-time switch
            {
                case 0b11: DLOG_trace3 << "\t[" << field<_packet_index_> (ctx) << ", " << print_timestamp (static_cast<timestamp_t> (field<_ts_> (ctx))) << "]: HEARTBEAT"; break;
                case 0b10: DLOG_trace3 << "\t[" << field<_packet_index_> (ctx) << "]: HEARTBEAT"; break;
                case 0b01: DLOG_trace3 << "\t[" << print_timestamp (static_cast<timestamp_t> (field<_ts_> (ctx))) << "]: HEARTBEAT"; break;
                default:   DLOG_trace3 << "\tHEARTBEAT"; break;

            } // end of switch
        }

        template<typename CTX>
        void
        visit_SoupTCP_login_request (CTX & ctx, SoupTCP_packet_hdr const & packet_hdr, SoupTCP_login_request const & msg)
        {
            // [no-op: subclasses can override]

            using io::net::_packet_index_;
            using _ts_          = select_display_ts_t<CTX>;

            constexpr bool have_ts              = has_field<_ts_, CTX> ();
            constexpr bool have_packet_index    = has_field<_packet_index_, CTX> ();

            switch ((have_packet_index << 1) | have_ts) // compile-time switch
            {
                case 0b11: DLOG_trace1 << "\t[" << field<_packet_index_> (ctx) << ", " << print_timestamp (static_cast<timestamp_t> (field<_ts_> (ctx))) << "]: login requested: " << msg; break;
                case 0b10: DLOG_trace1 << "\t[" << field<_packet_index_> (ctx) << "]: login requested: " << msg; break;
                case 0b01: DLOG_trace1 << "\t[" << print_timestamp (static_cast<timestamp_t> (field<_ts_> (ctx))) << "]: login requested: " << msg; break;
                default:   DLOG_trace1 << "\tlogin requested: " << msg; break;

            } // end of switch
        }

}; // end of class

} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
