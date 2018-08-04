#pragma once

#include "vr/fields.h"
#include "vr/market/sources/asx/ouch/OUCH_visitor.h"
#include "vr/util/datetime.h"
#include "vr/util/logging.h"

#include <bitset>

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
namespace ASX
{
//............................................................................
//............................................................................
namespace impl
{

class OUCH_printer_base
{
    protected: // ............................................................

        OUCH_printer_base (arg_map const & args) :
            m_out { * args.get<std::ostream *> ("out") },
            m_date { args.get<util::date_t> ("date") },
            m_tz { args.get<std::string> ("tz") },
            m_tz_offset { util::tz_offset (m_date, m_tz) },
            m_print_prefix { args.get<bool> ("prefix", true) },
            m_pf { args.get<bitset32_t> ("partitions", static_cast<bitset32_t> (-1)) }
        {
            util::time_diff_t const offset { pt::seconds { m_tz_offset / 1000000000 } + util::subsecond_duration_ns { m_tz_offset % 1000000000 } };

            LOG_info << "using " << print (m_tz) << " (offset " << offset << "), partition filter " << std::bitset<partition_count()> { m_pf };
        }


        VR_FORCEINLINE bool pix_included (int32_t const pix)
        {
            return (m_pf & (1 << pix));
        }

        template<typename CTX>
        VR_ASSUME_HOT void prefix (CTX const & ctx, int32_t const pix)
        {
            static constexpr bool have_ts_origin    = has_field<_ts_origin_, CTX> ();
            static constexpr bool have_ts_local     = has_field<_ts_local_, CTX> ();
            static constexpr bool have_seqnum       = has_field<_seqnum_, CTX> ();

            std::ostream & out = m_out;

            auto const pkt              = field<io::net::_packet_index_> (ctx);

            out << '[' << pkt << ", P" << pix;

            timestamp_t ts_origin { };
            if (have_ts_origin) // compile-time branch
            {
                ts_origin               = field<_ts_origin_> (ctx);

                out << ", " << print_timestamp (ts_origin, m_tz_offset);
            }

            if (have_ts_local) // compile-time branch
            {
                auto const ts_local     = field<_ts_local_> (ctx);

                out << ", " << print_timestamp (ts_local, m_tz_offset);

                if (have_ts_origin) // compile-time branch
                {
                    auto const diff = (ts_local - ts_origin);

                    out << " (" << (diff >= 0 ? '+' : '-') << (ts_local - ts_origin) << ')';
                }
            }

            if (have_seqnum) // compile-time branch
            {
                auto const sn           = field<_seqnum_> (ctx);

                out << ", sn " << sn;
            }

            out << "]\n  ";
        }


        std::ostream & m_out;
        util::date_t const m_date;
        std::string const m_tz;
        timestamp_t const m_tz_offset;
        bool const m_print_prefix;
        bitset32_t const m_pf;

}; // end of class

} // end of 'impl'
//............................................................................
//............................................................................
/**
 * @see ouch::print_message() in "messages_io.h"
 */
template<io::mode::enum_t IO_MODE, typename CTX>
class OUCH_printer; // master


#define vr_PRINT_BODY() \
            int32_t const pix  = field<_partition_> (ctx); \
            \
            if (pix_included (pix)) \
            { \
                if (m_print_prefix) prefix (ctx, pix); \
                m_out << msg << std::endl; \
            } \
            /* */

#define vr_VISIT_MESSAGE(r, unused, MSG) \
        VR_ASSUME_HOT bool visit (ouch:: MSG const & msg, CTX & ctx) /* override */ \
        { \
            auto const rc = super::visit (msg, ctx); /* [chain] */ \
            \
            vr_PRINT_BODY () \
            \
            return rc; \
        } \
        /* */

//............................................................................

template<typename CTX> // 'recv' specialization
class OUCH_printer<io::mode::recv, CTX>: public impl::OUCH_printer_base, public OUCH_visitor<io::mode::recv, OUCH_printer<io::mode::recv, CTX>>
{
    private: // ..............................................................

        using super         = OUCH_visitor<io::mode::recv, OUCH_printer<io::mode::recv, CTX>>;

        vr_static_assert (has_field<_ts_origin_, CTX> ()); // for 'recv'

    public: // ...............................................................

        OUCH_printer (arg_map const & args) :
            impl::OUCH_printer_base (args),
            super (args)
        {
        }

        // overridden OUCH visits:

        using super::visit;

#   define vr_MESSAGE_SEQ   VR_MARKET_OUCH_RECV_MESSAGE_SEQ

        BOOST_PP_SEQ_FOR_EACH (vr_VISIT_MESSAGE, unused, vr_MESSAGE_SEQ)

#   undef vr_MESSAGE_SEQ

}; // end of class
//............................................................................

template<typename CTX> // 'send' specialization
class OUCH_printer<io::mode::send, CTX>: public impl::OUCH_printer_base, public OUCH_visitor<io::mode::send, OUCH_printer<io::mode::send, CTX>>
{
    private: // ..............................................................

        using super         = OUCH_visitor<io::mode::send, OUCH_printer<io::mode::send, CTX>>;

    public: // ...............................................................

        OUCH_printer (arg_map const & args) :
            impl::OUCH_printer_base (args),
            super (args)
        {
        }

        // overridden OUCH visits:

        using super::visit;

#   define vr_MESSAGE_SEQ   VR_MARKET_OUCH_SEND_MESSAGE_SEQ

        BOOST_PP_SEQ_FOR_EACH (vr_VISIT_MESSAGE, unused, vr_MESSAGE_SEQ)

#   undef vr_MESSAGE_SEQ

}; // end of class
//............................................................................

#undef vr_VISIT_MESSAGE
#undef vr_PRINT_BODY

} // end of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
