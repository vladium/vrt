#pragma once

#include "vr/arg_map.h"
#include "vr/fields.h"
#include "vr/market/sources/asx/ouch/messages.h"
#include "vr/util/datetime.h" // print_timestamp
#include "vr/util/logging.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
namespace ASX
{
//............................................................................

template<io::mode::enum_t IO_MODE, typename DERIVED = void> // a slight twist on CRTP
class OUCH_visitor; // master

//............................................................................

#define vr_NULL_VISIT(r, unused, MSG) \
        template<typename CTX> \
        VR_FORCEINLINE bool visit (ouch:: MSG const & msg, CTX & ctx) \
        { \
            return true; /* null version does not filter */ \
        } \
        /* */

//............................................................................

template<typename DERIVED> // 'recv' specialization
class OUCH_visitor<io::mode::recv, DERIVED>
{
    private: // ..............................................................

        static constexpr io::mode::enum_t mode ()   { return io::mode::recv; }

        using this_type     = OUCH_visitor<mode (), DERIVED>;
        using derived       = util::this_or_derived_t<this_type, DERIVED>; // a slight twist on CRTP

    public: // ...............................................................

        OUCH_visitor ()     = default;

        OUCH_visitor (arg_map const & args) :
            OUCH_visitor { }
        {
        }

        // overridable hooks:

        template<typename CTX>
        VR_FORCEINLINE bool visit (pre_message const msg_type, CTX & ctx)
        {
            vr_static_assert (std::is_base_of<this_type, derived>::value);

            return true; // null version does not filter
        }

        template<typename CTX>
        VR_FORCEINLINE void visit (post_message const msg_type, CTX & ctx)
        {
            vr_static_assert (std::is_base_of<this_type, derived>::value);

            // [null version is a no-op]
        }

#   define vr_MESSAGE_SEQ   VR_MARKET_OUCH_RECV_MESSAGE_SEQ

        // overridable visits:

        BOOST_PP_SEQ_FOR_EACH (vr_NULL_VISIT, unused, vr_MESSAGE_SEQ)

    protected: // ............................................................

        template<typename CTX>
        VR_ASSUME_HOT void _internal_message_visit (addr_const_t const data, CTX & ctx)
        {
            vr_static_assert (std::is_base_of<this_type, derived>::value);

            ouch::OUCH_hdr const & hdr = (* static_cast<ouch::OUCH_hdr const *> (data));
            ouch::recv_message_type::enum_t const msg_type = hdr.type ();

            if (has_field<_ts_origin_, CTX> ()) // propagate OUCH header timestamp to further visits
            {
                timestamp_t const ts_utc = hdr.ts ();
                field<_ts_origin_> (ctx) = ts_utc;

                DLOG_trace2 << "OUCH " << mode () << " message type: " << print (static_cast<char> (msg_type)) << " (" << print (msg_type) << "), ts_origin: " << print_timestamp (ts_utc);
            }
            else
            {
                DLOG_trace2 << "OUCH " << mode () << " message type: " << print (static_cast<char> (msg_type)) << " (" << print (msg_type) << ')';
            }

            if (static_cast<derived *> (this)->visit (pre_message { msg_type }, ctx)) // pre-visit
            {
                switch (msg_type)
                {
#               define vr_DISPATCH_CASE(r, unused, MSG) \
                    case ouch::message_type<mode ()>:: MSG : \
                    { \
                        ouch:: MSG const & msg = * static_cast<ouch:: MSG const *> (data); \
                        DLOG_trace3 << msg; \
                        \
                        static_cast<derived *> (this)->visit (msg, ctx); \
                    } \
                    break; \
                    /* */

                    BOOST_PP_SEQ_FOR_EACH (vr_DISPATCH_CASE, unused, vr_MESSAGE_SEQ)

                    default:  VR_ASSUME_UNREACHABLE (hdr.type ());

#               undef vr_DISPATCH_CASE
                } // end of switch

                static_cast<derived *> (this)->visit (post_message { msg_type }, ctx); // post-visit
            }
        }

#   undef vr_MESSAGE_SEQ

}; // end of specialization
//............................................................................

template<typename DERIVED> // 'send' specialization
class OUCH_visitor<io::mode::send, DERIVED>
{
    private: // ..............................................................

        static constexpr io::mode::enum_t mode ()   { return io::mode::send; }

        using this_type     = OUCH_visitor<mode (), DERIVED>;
        using derived       = util::this_or_derived_t<this_type, DERIVED>; // a slight twist on CRTP

    public: // ...............................................................

        OUCH_visitor ()     = default;

        OUCH_visitor (arg_map const & args) :
            OUCH_visitor { }
        {
        }

        // overridable hooks:

        template<typename CTX>
        VR_FORCEINLINE bool visit (pre_message const msg_type, CTX & ctx)
        {
            vr_static_assert (std::is_base_of<this_type, derived>::value);

            return true; // null version does not filter
        }

        template<typename CTX>
        VR_FORCEINLINE void visit (post_message const msg_type, CTX & ctx)
        {
            vr_static_assert (std::is_base_of<this_type, derived>::value);

            // [null version is a no-op]
        }

#   define vr_MESSAGE_SEQ   VR_MARKET_OUCH_SEND_MESSAGE_SEQ

        // overridable visits:

        BOOST_PP_SEQ_FOR_EACH (vr_NULL_VISIT, unused, vr_MESSAGE_SEQ)

    protected: // ............................................................

        /*
         * this is not meant to be overridable; sandwiches a CRTP-overriddable
         * 'visit(msg, ctx)' between 'pre_message' and 'post_message' visit hooks
         */
        template<typename CTX>
        VR_ASSUME_HOT void _internal_message_visit (addr_const_t const data, CTX & ctx)
        {
            vr_static_assert (std::is_base_of<this_type, derived>::value);

            ouch::OUCH_hdr const & hdr = (* static_cast<ouch::OUCH_hdr const *> (data));
            int32_t const msg_type = hdr.type ();

            DLOG_trace2 << "OUCH " << mode () << " message type: " << print (static_cast<char> (msg_type));

            if (static_cast<derived *> (this)->visit (pre_message { msg_type }, ctx)) // pre-visit
            {
                switch (msg_type)
                {
#               define vr_DISPATCH_CASE(r, unused, MSG) \
                    case ouch::message_type<mode ()>:: MSG : \
                    { \
                        ouch:: MSG const & msg = * static_cast<ouch:: MSG const *> (data); \
                        DLOG_trace3 << msg; \
                        \
                        static_cast<derived *> (this)->visit (msg, ctx); \
                    } \
                    break; \
                    /* */

                    BOOST_PP_SEQ_FOR_EACH (vr_DISPATCH_CASE, unused, vr_MESSAGE_SEQ)

                    default:  VR_ASSUME_UNREACHABLE (hdr.type ());

#               undef vr_DISPATCH_CASE
                } // end of switch

                static_cast<derived *> (this)->visit (post_message { msg_type }, ctx); // post-visit
            }
        }

#   undef vr_MESSAGE_SEQ

}; // end of specialization
//............................................................................

#undef vr_NULL_VISIT

} // end of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
