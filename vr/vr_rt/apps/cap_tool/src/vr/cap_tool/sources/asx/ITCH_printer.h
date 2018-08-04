#pragma once

#include "vr/fields.h"
#include "vr/market/sources/asx/itch/ITCH_ts_tracker.h"
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

class ITCH_printer_base
{
    protected: // ............................................................

        ITCH_printer_base (arg_map const & args) :
            m_out { * args.get<std::ostream *> ("out") },
            m_date { args.get<util::date_t> ("date") },
            m_tz { args.get<std::string> ("tz") },
            m_tz_offset { util::tz_offset (m_date, m_tz) },
            m_print_seconds { !! (message_filter_arg (args) & (_one_128 () << itch::message_type::seconds)) },
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

        static bitset128_t message_filter_arg (arg_map const & args)
        {
            return args.get<bitset128_t> ("messages", -1);
        }


        std::ostream & m_out;
        util::date_t const m_date;
        std::string const m_tz;
        timestamp_t const m_tz_offset;
        bool const m_print_seconds;
        bool const m_print_prefix;
        bitset32_t const m_pf;

}; // end of class

} // end of 'impl'
//............................................................................
//............................................................................
/**
 * @see itch::print_message() in "messages_io.h"
 */
template<typename CTX>
class ITCH_printer: public impl::ITCH_printer_base, public ITCH_ts_tracker<CTX, ITCH_printer<CTX>>
{
    private: // ..............................................................

        using super         = ITCH_ts_tracker<CTX, ITCH_printer<CTX>>;

        vr_static_assert (has_field<io::net::_packet_index_, CTX> ());
        vr_static_assert (has_field<_ts_origin_, CTX> ());

    public: // ...............................................................

        ITCH_printer (arg_map const & args) :
            impl::ITCH_printer_base (args),
            super (args)
        {
        }

        // overridden ITCH visits:

        using super::visit;

#define vr_PRINT_BODY() \
            int32_t const pix  = field<_partition_> (ctx); \
            \
            if (pix_included (pix)) \
            { \
                if (m_print_prefix) prefix (ctx, pix); \
                m_out << msg << std::endl; \
            } \
            /* */

        /*
         * 'seconds' is special:
         *
         *  always chain to super but display only if explicitly chosen
         */
        VR_ASSUME_HOT bool visit (itch::seconds const & msg, CTX & ctx) // override
        {
            auto const rc = super::visit (msg, ctx); // [chain]

            if (m_print_seconds)
            {
                vr_PRINT_BODY ()
            }

            return rc;
        }


#define vr_VISIT_MESSAGE(r, unused, MSG) \
        VR_ASSUME_HOT bool visit (itch:: MSG const & msg, CTX & ctx) /* override */ \
        { \
            auto const rc = super::visit (msg, ctx); /* [chain] */ \
            \
            vr_PRINT_BODY () \
            \
            return rc; \
        } \
        /* */


        BOOST_PP_SEQ_FOR_EACH (vr_VISIT_MESSAGE, unused, BOOST_PP_SEQ_TAIL (VR_MARKET_ITCH_RECV_MESSAGE_SEQ)) // skip 'seconds'


#undef vr_VISIT_MESSAGE
#undef vr_PRINT_BODY

    private: // ..............................................................

        VR_ASSUME_HOT void prefix (CTX & ctx, int32_t const pix)
        {
            static constexpr bool have_ts_local     = has_field<_ts_local_, CTX> ();
            static constexpr bool have_seqnum       = has_field<_seqnum_, CTX> ();

            std::ostream & out = m_out;

            auto const pkt          = field<io::net::_packet_index_> (ctx);
            auto const ts_origin    = field<_ts_origin_> (ctx); // always present

            out << '[' << pkt << ", P" << pix << ", " << print_timestamp (ts_origin, m_tz_offset);

            if (have_ts_local) // compile-time branch
            {
                auto const ts_local = field<_ts_local_> (ctx);
                auto const diff = (ts_local - ts_origin);

                out << ", " << print_timestamp (ts_local, m_tz_offset) << " (" << (diff >= 0 ? '+' : '-') << (ts_local - ts_origin) << ')';
            }

            if (have_seqnum) // compile-time branch
            {
                auto const sn       = field<_seqnum_> (ctx);

                out << ", sn " << sn;
            }

            out << "]\n  ";
        }

}; // end of class

} // end of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
