#pragma once

#include "vr/arg_map.h"
#include "vr/fields.h"
#include "vr/io/net/defs.h" // _packet_index_
#include "vr/market/sources/asx/itch/messages.h"
#include "vr/util/logging.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
namespace ASX
{
/**
 * a null ITCH message visitor that implements '_internal_message_visit()' to demultiplex
 * messages onto a set of overridable 'visit(<ITCH message>)' methods
 *
 * (the base implementations of the latter in this class are empty and return 'true')
 *
 * message processing stacks can be built in a couple of different (and not mutually-exclusive)
 * paradigms:
 *
 *  1. "vertically", by building a CRTP-inheritance chain rooted at 'ITCH_visitor' with judicious 'visit()' overriding
 *     and chaining to super visits;
 *
 *  2. "horizontally", by building a @ref ITCH_pipeline to form a linear sequence of (possibly) unrelated classes
 *
 * @note all "visit" methods are intentionally overloaded via signatures only to ease "using"-style
 * declarations in templated subclasses
 *
 * TODO doc processing invariants, pre/post-visit hooks, etc
 */
template<typename DERIVED = void> // a slight twist on CRTP
class ITCH_visitor
{
    private: // ..............................................................

        using this_type     = ITCH_visitor<DERIVED>;
        using derived       = util::if_is_void_t<DERIVED, this_type, DERIVED>; // a slight twist on CRTP

    public: // ...............................................................

        ITCH_visitor ()     = default;

        ITCH_visitor (arg_map const & args) :
            ITCH_visitor { }
        {
        }

        // overridable hooks:

        template<typename CTX>
        VR_FORCEINLINE void visit (pre_packet const msg_count, CTX & ctx)
        {
            // [null version is a no-op]
        }

        template<typename CTX>
        VR_FORCEINLINE void visit (post_packet const msg_count, CTX & ctx)
        {
            // [null version is a no-op]
        }

        /**
         * this method is invoked before a possible 'visit(msg_type, ctx)'
         *
         * @param msg_type ITCH message type being considered for proper 'visit()'
         * @return if 'false', this visitor (possibly derived from) will
         *         not see the full message via 'visit(msg, ctx)' and will not post-visit it
         *
         * @invariant if pre-visit returns 'false', the corresponding 'visit(msg, ctx)' and 'visit(post_message { msg_type }, ctx)' will *not* be invoked
         * @invariant visit(msg) and visit(post_message) occur or don't occur together
         */
        template<typename CTX>
        VR_FORCEINLINE bool visit (pre_message const msg_type, addr_const_t const msg, CTX & ctx)
        {
            return true; // null version does not filter
        }
        /**
         * this method is invoked after and only if 'visit(msg, ctx)' has been invoked
         *
         * @param msg_type ITCH message type that was passed into the pre-visit
         */
        template<typename CTX>
        VR_FORCEINLINE void visit (post_message const msg_type, CTX & ctx)
        {
            // [null version is a no-op]
        }

        // overridable visits:

#define vr_NULL_VISIT(r, unused, MSG) \
        template<typename CTX> \
        VR_FORCEINLINE bool visit (itch:: MSG const & msg, CTX & ctx) \
        { \
            return true; /* null version does not filter */ \
        } \
        /* */


        BOOST_PP_SEQ_FOR_EACH (vr_NULL_VISIT, unused, VR_MARKET_ITCH_RECV_MESSAGE_SEQ)


#undef vr_NULL_VISIT

    protected: // ............................................................

        /*
         * this is not meant to be overridable; sandwiches a CRTP-overriddable
         * 'visit(msg, ctx)' between 'pre_message' and 'post_message' visit hooks
         */
        template<typename CTX>
        VR_ASSUME_HOT void _internal_message_visit (addr_const_t const data, CTX & ctx)
        {
            vr_static_assert (std::is_base_of<this_type, derived>::value);

            itch::message_type::enum_t const msg_type = (* static_cast<itch::message_type::enum_t const *> (data));

            DLOG_trace2 << "ITCH message type: " << static_cast<char> (msg_type) << " (" << print (msg_type) << ')';

            if (static_cast<derived *> (this)->visit (pre_message { msg_type }, data, ctx)) // pre-visit
            {
                switch (msg_type) // visit, possibly overridden
                {
#               define vr_DISPATCH_CASE(r, unused, MSG) \
                    case itch::message_type:: MSG : \
                    { \
                        itch:: MSG const & msg = * static_cast<itch:: MSG const *> (data); \
                        DLOG_trace3 << msg; \
                        \
                        static_cast<derived *> (this)->visit (msg, ctx); \
                    } \
                    break; \
                    /* */

                    BOOST_PP_SEQ_FOR_EACH (vr_DISPATCH_CASE, unused, VR_MARKET_ITCH_RECV_MESSAGE_SEQ)

                    default:  VR_ASSUME_UNREACHABLE (msg_type);

#               undef vr_DISPATCH_CASE
                } // end of switch

                static_cast<derived *> (this)->visit (post_message { msg_type }, ctx); // post-visit
            }
        }

        /*
         * this is not meant to be overridable (a DLOG_trace wrapper that will be invoked once per packet)
         *
         * GLOG_vmodule=ITCH_visitor=1,market_data_view=3 ... is handy
         */
        template<typename CTX>
        VR_FORCEINLINE void _internal_packet_mark (pre_packet const msg_count, CTX & ctx)
        {
            vr_static_assert (std::is_base_of<this_type, derived>::value);

            VR_IF_DEBUG
            (
                constexpr bool has_ts_local_delta   = has_field<io::_ts_local_delta_, CTX> ();
                constexpr bool has_packet_index     = has_field<io::net::_packet_index_, CTX> ();

                int64_t const pkt { has_packet_index ? field<io::net::_packet_index_> (ctx) : 0 };
                timestamp_t const ts_delta { has_ts_local_delta ? field<io::_ts_local_delta_> (ctx) : 0 };

                switch ((has_packet_index << 1) | has_ts_local_delta) // compile-time switch
                {
                    case 0b11: DLOG_trace1 << "[--- start of packet #" << pkt << " (" << msg_count << " msg(s), +" << ts_delta << " ns) ---]"; break;
                    case 0b01: DLOG_trace1 << "[--- start of packet (" << msg_count << " msg(s), +" << ts_delta << " ns) ---]"; break;
                    case 0b10: DLOG_trace1 << "[--- start of packet #" << pkt << " (" << msg_count << " msg(s)) ---]"; break;
                    default:   DLOG_trace1 << "[--- start of packet (" << msg_count << " msg(s)) ---]"; break;

                } // end of switch
            )

            static_cast<derived *> (this)->visit (msg_count, ctx); // possibly overridden
        }

        /*
         * this is not meant to be overridable (a DLOG_trace wrapper that will be invoked once per packet)
         */
        template<typename CTX>
        VR_FORCEINLINE void _internal_packet_mark (post_packet const msg_count, CTX & ctx)
        {
            vr_static_assert (std::is_base_of<this_type, derived>::value);

            static_cast<derived *> (this)->visit (msg_count, ctx); // possibly overridden

            VR_IF_DEBUG
            (
                constexpr bool has_packet_index = has_field<io::net::_packet_index_, CTX> ();

                int64_t const pkt { has_packet_index ? field<io::net::_packet_index_> (ctx) : 0 };

                switch (has_packet_index) // compile-time switch
                {
                    case 0b01: DLOG_trace2 << "[--- end of packet #" << pkt << " (" << msg_count << " msg(s)) ---]"; break;
                    default:   DLOG_trace2 << "[--- end of packet (" << msg_count << " msg(s)) ---]"; break;

                } // end of switch
            )
        }

}; // end of class

} // end of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
