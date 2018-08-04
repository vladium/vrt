
#include "vr/market/rt/agents/asx/execution_manager.h"

#include "vr/market/rt/cfg/agent_cfg.h"
#include "vr/rt/cfg/app_cfg.h"
#include "vr/settings.h"
#include "vr/util/parse.h"
#include "vr/util/random.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
namespace ASX
{
//............................................................................

execution_manager::execution_manager (agent_ID const & ID) :
    view (),
    super (book ()),
    m_ID { ID }
{
    check_nonempty (m_ID);
}
//............................................................................

void
execution_manager::start (rt::app_cfg const & config, agent_cfg const & agents)
{
    m_otk_counter = std::max<uint32_t> (1, config.start_time ().time_of_day ().total_seconds ()); // try to be "monotonic" throughout the day

    // get our traded instruments from agent cfg:

    symbol_liid_relation const & slr = agents.agents ()[m_ID].m_symbol_mapping;

    for (auto const & sl : slr.left)
    {
        liid_t const & liid = sl.second;

        LOG_info << "  symbol/liid mapping: " << print (sl.first) << " -> " << liid;

        m_liids.push_back (liid);
    }
    assert_nonempty (m_liids);

    // note: 'agent_cfg' randomizes liid mappings, here we also randomize our symbol iteration order:
    {
        uint64_t const rng_seed = config.rng_seed ();
        LOG_trace1 << '[' << print (m_ID) << ": seed = " << rng_seed << ']';

        util::random_shuffle (& m_liids [0], m_liids.size (), rng_seed);
    }

    // config seems ok, now connect to execution:

    auto ifc = m_xl->connect (m_ID);
    {
        m_ex_queue = & ifc.queue ();
        m_partition_mask = ifc.partition_mask ();
        m_otk_prefix = ifc.otk_prefix ();
    }
    LOG_info << print (m_ID) << ": connected to execution link @ " << m_ex_queue << ", partition mask: " << std::bitset<part_count> (m_partition_mask);

    check_nonnull (m_ex_queue);
    check_nonzero (m_partition_mask);
    check_nonzero (m_otk_counter);
    check_in_inclusive_range (m_otk_prefix, 0x0, 0xF);

    LOG_info << "configured with " << m_liids.size () << " traded instrument(s)";
}
//............................................................................

void
execution_manager::visit_SoupTCP_login (visit_ctx & ctx, SoupTCP_packet_hdr const & soup_hdr, SoupTCP_login_accepted const & msg) // override
{
    super::visit_SoupTCP_login (ctx, soup_hdr, msg); // [chain]

    vr_static_assert (has_field<_ts_local_, visit_ctx> ());
    vr_static_assert (has_field<_partition_, visit_ctx> ());

    int32_t const pix = field<_partition_> (ctx);
    LOG_info << print (m_ID) << ": P" << pix << " login accepted @ " << print_timestamp (field<_ts_local_> (ctx)) << ": " << msg;

    check_nonzero (m_login_pending_mask & (1 << pix));
    m_login_pending_mask &= ~(1 << pix); // clear the pending bit

    session_context & s = m_sessions [pix];

    s.m_session_name.assign (msg.session ().data (), msg.session ().size ());
    {
        auto const msg_seqnum = util::ltrim (msg.seqnum ());
        if (VR_LIKELY (! msg_seqnum.empty ()))
        {
            s.m_snapshot_seqnum = util::parse_decimal<int64_t> (msg_seqnum.m_start, msg_seqnum.m_size);
            LOG_trace1 << "  " << print (m_ID) << ": P" << pix << " session " << print (s.m_session_name) << " seqnum set to " << s.m_snapshot_seqnum;
        }
    }
}

void
execution_manager::visit_SoupTCP_login (visit_ctx & ctx, SoupTCP_packet_hdr const & soup_hdr, SoupTCP_login_rejected const & msg) // override
{
    vr_static_assert (has_field<_partition_, visit_ctx> ());

    // not supposed to occur for this particular test:

    throw_x (illegal_state, print (m_ID) + ": P" + string_cast (field<_partition_> (ctx)) + " login REJECTED: " + print (msg));
}

void
execution_manager::login_transition ()
{
    switch (m_state)
    {
        case state::start:
        {
            auto e = m_ex_queue->try_enqueue ();
            if (e)
            {
                xl_request & req = xl_request_cast (e);
                {
                    field<_type_> (req) = xl_req::start_login;
                }
                e.commit ();
                LOG_info << print (m_ID) << ": sent login request";

                m_login_pending_mask = m_partition_mask;
                m_state = state::login_barrier;
            }
        }
        break;

        case state::login_barrier:
        {
            // [waiting for acks from all active partitions to transition us to 'running']

            if (! m_login_pending_mask) // partition bits are cleared by 'visit_SoupTCP_login()'
            {
                m_state = state::running;
                LOG_info << " ======[" << print (m_ID) <<  " login ok, transitioning to " << print (m_state) << "]======";
            }
        }
        break;

        default: VR_ASSUME_UNREACHABLE (m_state);

    } // end of switch
}

} // end of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
