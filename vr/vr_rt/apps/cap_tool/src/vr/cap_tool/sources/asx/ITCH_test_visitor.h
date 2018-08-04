#pragma once

#include "vr/market/sources/asx/itch/ITCH_ts_tracker.h"
#include "vr/stats/stream_stats.h"
#include "vr/util/datetime.h"

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

std::vector<double> const ss_probs = { 0.25, 0.5, 0.75, 0.99, 0.999 };

struct partition_test_context
{
    timestamp_t m_ts_local { };
    timestamp_t m_ts_origin { };
    mold_seqnum_t m_seqnum { };
    int32_t m_message_index { };
    bool m_date_checked { false };
    stats::stream_stats<timestamp_t> m_ss { ss_probs };

}; // end of class

} // end of 'impl'
//............................................................................
//............................................................................

template<typename CTX>
class ITCH_test_visitor: public ITCH_ts_tracker<CTX, ITCH_test_visitor<CTX> >
{
    private: // ..............................................................

        vr_static_assert (has_field<io::net::_packet_index_, CTX> ());
        vr_static_assert (has_field<_ts_origin_, CTX> ());

        vr_static_assert (has_field<_partition_, CTX> ());

        using super         = ITCH_ts_tracker<CTX, ITCH_test_visitor<CTX> >;

        static constexpr int32_t digits_secs ()     { return 9; }

    public: // ...............................................................

        static constexpr bool have_ts_local ()      { return has_field<_ts_local_, CTX> (); }
        static constexpr bool have_seqnum ()        { return has_field<_seqnum_, CTX> (); }

        static constexpr bool is_mcast ()           { return have_ts_local (); }


        ITCH_test_visitor (arg_map const & args) :
            super (args),
            m_date { args.get<util::date_t> ("date") },
            m_tz { args.get<std::string> ("tz") },
            m_tz_offset { util::tz_offset (m_date, m_tz) }
        {
            util::time_diff_t const offset { pt::seconds { m_tz_offset / 1000000000 } + util::subsecond_duration_ns { m_tz_offset % 1000000000 } };

            LOG_info << "using " << print (m_tz) << " (offset " << offset << "), assuming mcast: " << std::boolalpha << is_mcast () << ", testing capture timestamps: " << have_ts_local ();
        }

        ~ITCH_test_visitor ()
        {
            LOG_info << (m_error_count ? "*** ERROR COUNT: " : "error count: ") << m_error_count;

            if (m_last_gap_packet_index)
            {
                LOG_info << "initial GAP extent: " << m_last_gap_packet_index << " packet(s)";
            }

            for (int32_t pix = 0; pix < partition_count (); ++ pix)
            {
                auto const & ss = m_partitions [pix].m_ss;
                if (ss.count () > 300)
                {
                    LOG_info << "[partition " << pix << "]: " << ss.count () << " sample(s)";

                    for (int32_t p = 0; p < ss.size (); ++ p)
                    {
                        LOG_info << "  q " << impl::ss_probs [p] << ":\t" << ss [p];
                    }
                }
            }
        }

        int32_t const & error_count () const
        {
            return m_error_count;
        }

        timestamp_t const & tz_offset () const
        {
            return m_tz_offset;
        }

        // overridden ITCH visits:

        using super::visit;

        /*
         * TODO
         * - stats for ts monotonicity violations
         */
        VR_ASSUME_HOT void visit (post_message const msg_type, CTX & ctx) // override
        {
//            auto const error_count_start = m_error_count;

            auto const pkt          = field<io::net::_packet_index_> (ctx);
            auto const ts_origin    = field<_ts_origin_> (ctx);
            auto const pix          = field<_partition_> (ctx);

            impl::partition_test_context & state = m_partitions [pix];


            if (is_mcast ()) // for glimpse, can't be certain when the snapshot was made
            {
                // check that date as inferred from ts_origin is consistent with our file naming scheme:

                if (VR_UNLIKELY (! state.m_date_checked))
                {
                    if (VR_LIKELY (ts_origin > 0)) // TODO rm guard (ASX-19)
                    {
                        util::date_t const pkt_date = util::to_ptime (ts_origin + m_tz_offset).date ();

                        if (VR_UNLIKELY (pkt_date != m_date))
                        {
                            ++ m_error_count;

                            LOG_error << prefix (ctx) << "unexpected packet date (" << pkt_date << "), expected " << m_date;
                        }

                        state.m_date_checked = true;
                    }
                }
            }


            bool block_boundary { true };

            if (have_seqnum ())
            {
                auto const sn   = field<_seqnum_> (ctx);
                block_boundary  = (state.m_seqnum != sn);

                // seqnums, packet drops

                if (VR_LIKELY (! block_boundary)) // next message within the current message block
                {
                    ++ state.m_message_index;
                }
                else // next message block, check for seqnum gaps:
                {
                    if (VR_LIKELY (state.m_seqnum != 0)) // not very first message for this partition
                    {
                        mold_seqnum_t const sn_expected = state.m_seqnum + state.m_message_index;

                        if (VR_UNLIKELY (sn != sn_expected))
                        {
                            ++ m_error_count;

                            m_last_gap_packet_index = pkt;

                            LOG_error << prefix (ctx) << "seqnum GAP (" << (sn - sn_expected) << "): " << sn << ", expected " << sn_expected;
                        }
                    }
                    else
                    {
                        LOG_info << "partition " << pix << ": starting pkt " << pkt << ", sn " << sn << ", ts.origin " << print_timestamp (ts_origin, m_tz_offset);
                    }

                    state.m_seqnum = sn;
                    state.m_message_index = 1;
                }
            }

            // ITCH timestamp should be weakly monotonic:
            {
//                if (VR_UNLIKELY (ts_origin < state.m_ts_origin))
//                    throw_x (invalid_input, "[pkt #" + string_cast (pkt) + ", partition " + string_cast (pix) + ", sn " + string_cast (sn) + ", " + util::print_timestamp_as_ptime (ts_origin, digits_secs ()) + "]:"
//                        " non-monotonic origin ts " + util::print_timestamp_as_ptime (ts_origin, digits_secs ())
//                        + ", prev " + util::print_timestamp_as_ptime (state.m_ts_origin, digits_secs ())
//                        + ", GAP " + string_cast (state.m_ts_origin - ts_origin) + " ns");

                state.m_ts_origin = ts_origin;
            }


            if (have_ts_local ())
            {
                timestamp_t const ts_local = field<_ts_local_> (ctx);

                // if capture ts is available, it should be monotonic across block boundaries:

                if (block_boundary | ! have_seqnum ()) // check ts_local whenever it has a chance to change
                {
                    if (VR_UNLIKELY (ts_local < state.m_ts_local))
                    {
                        ++ m_error_count;

                        LOG_error << prefix (ctx) << "non-monotonic capture ts " << print_timestamp (ts_local, m_tz_offset)
                                  << ", prev " << print_timestamp (state.m_ts_local, m_tz_offset)
                                  << ", GAP " << (state.m_ts_local - ts_local) << " ns";
                    }

                    state.m_ts_local = ts_local;
                }

                // update (ts_local - ts_origin) stats:

                if (VR_LIKELY (ts_origin > 0)) state.m_ss (ts_local - ts_origin); // TODO rm guard (ASX-19)
            }

//            if (VR_UNLIKELY (m_error_count > error_count_start)) // log message that triggered new error(s)
//            {
//                // TODO need to retain a ref to specific msg, possibly in 'CTX', during pre-visit ?
//            }
        }

    private: // ..............................................................

        struct print_prefix final
        {
            print_prefix (ITCH_test_visitor const & parent, CTX & ctx) :
                m_parent { parent },
                m_ctx { ctx }
            {
            }

            friend std::ostream & operator<< (std::ostream & os, print_prefix const & obj) VR_NOEXCEPT
            {
                CTX & ctx { obj.m_ctx };

                auto const pkt          = field<io::net::_packet_index_> (ctx);
                auto const ts_origin    = field<_ts_origin_> (ctx);
                auto const pix          = field<_partition_> (ctx);

                os << "[pkt #" << pkt << ", partition " << pix;
                if (have_seqnum ())
                {
                    auto const sn   = field<_seqnum_> (ctx);
                    os << ", sn " << sn;
                }
                os << ", " << print_timestamp (ts_origin, obj.m_parent.tz_offset ()) << "]: ";

                return os;
            }

            ITCH_test_visitor const & m_parent;
            CTX & m_ctx;

        }; // end of nested class

        VR_FORCEINLINE print_prefix prefix (CTX & ctx) const
        {
            return { * this, ctx };
        }


        util::date_t const m_date;
        std::string const m_tz;
        timestamp_t const m_tz_offset;
        int64_t m_last_gap_packet_index { };
        std::array<impl::partition_test_context, partition_count ()> m_partitions;
        int32_t m_error_count { };

}; // end of class

} // end of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
