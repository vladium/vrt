
#include "vr/market/rt/asx/execution_link.h"

#include "vr/io/events/event_context.h"
#include "vr/io/links/link_factory.h"
#include "vr/io/links/TCP_link.h"
#include "vr/market/net/SoupTCP_.h"
#include "vr/market/ref/asx/ref_data.h"
#include "vr/market/rt/cfg/agent_cfg.h"
#include "vr/market/rt/impl/reclamation.h" // release_poll_descriptor()
#include "vr/market/sources/asx/ouch/messages.h"
#include "vr/mc/atomic.h"
#include "vr/mc/mc.h"
#include "vr/mc/thread_pool.h"
#include "vr/rt/cfg/app_cfg.h"
#include "vr/settings.h"
#include "vr/util/format.h"
#include "vr/util/object_pools.h"
#include "vr/util/ops_int.h"
#include "vr/util/random.h" // random_shuffle

#include <algorithm>
#include <bitset>
#include <set>

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{

template<std::size_t N> // TODO this needs to be in the same ns as "real" version, but move elsewhere
inline void
copy_to_alphanum (fw_string<N> const & src, meta::impl::dummy_vholder::value_type & dst)
{
    VR_ASSUME_UNREACHABLE ();
}


namespace ASX
{
using namespace io;
namespace m     = meta;

//............................................................................

vr_static_assert (std::is_standard_layout<recv_reclaim_context>::value);

vr_static_assert (alignof (execution_link::poll_descriptor) == sys::cpu_info::cache::static_line_size ());

//............................................................................

using int_ops           = util::ops_int<util::arg_policy<util::zero_arg_policy::ignore, 0>, false>; // unchecked

static constexpr int32_t part_count ()          { return execution_link::poll_descriptor::width (); }

static constexpr int32_t login_size ()          { return (sizeof (SoupTCP_packet_hdr) + sizeof (SoupTCP_login_request)); }

// price conversion b/w 'xl_request' and what this source uses

using price_traits      = this_source_traits::price_traits<meta::find_field_def_t<_price_, xl_request>::value_type>;

//............................................................................

using liid_map_entry    = m::make_compact_struct_t<m::make_schema_t
                        <
                            m::fdef_<iid_t,     _iid_>,
                            m::fdef_<int32_t,   _partition_>
                        >>;
//............................................................................

using visit_ctx         = event_context<_ts_local_>;

struct null_encapsulated    { };

//............................................................................

struct execution_link::pimpl final: public SoupTCP_<mode::recv, null_encapsulated>
{
    using super         = SoupTCP_<mode::recv, null_encapsulated>; // a minimal setup just to see Soup framing

    static constexpr int32_t min_available ()   { return io::net::min_size_or_zero<super>::value (); }

    pimpl (execution_link & parent, scope_path const & cfg_path) :
        m_parent { parent },
        m_published { parent.m_published.value () },
        m_cfg_path { cfg_path }
    {
    }

    void initialize () // part of constructor code that needs to be a separate method to work around init ordering
    {
        {
            auto const ar = m_pds.allocate ();

            poll_descriptor & pd { std::get<0> (ar) };

            // initialize since this is the starting published state:
            {
                pd.m_parent_reclaim_head = & m_pd_reclaim_head.value ();
                pd.m_instance = std::get<1> (ar);
                pd.m_next = -1;
                pd.clear ();
            }

            mc::volatile_cast (m_published) = & pd;
        }
        {
            auto const ar = m_pds.allocate ();

            poll_descriptor & pd { std::get<0> (ar) };

            pd.m_parent_reclaim_head = & m_pd_reclaim_head.value();
            pd.m_instance = std::get<1> (ar);
            pd.m_next = -1;
            __builtin_memcpy (pd.data (), m_published->data (), sizeof (poll_descriptor::data_array)); // know it's all zeroes, but for clarity

            m_current = & pd;
        }

        assert_ne (m_published, m_current); // invariant
    }


    VR_ASSUME_COLD void start ()
    {
        rt::app_cfg const & config = (* m_parent.m_config);

        LOG_trace1 << "session start time: " << config.start_time ();

        settings const & cfg = config.scope (m_cfg_path);
        LOG_trace1 << "using cfg:\n" << print (cfg);
        check_condition (cfg.is_object (), cfg.type ());

        bitset32_t pix_mask { }; // will be inferred from agent cfs below
        {
            // iterate over agent cfg(s) and configure:
            //
            //  - 'm_liid_limit': local copy of 'agent_cfg::liid_limit()'
            //  - 'm_liid_map': indexed by liid like liid_table but contains data for actual OUCH interaction, iids and partitions
            //  - 'm_active_partitions': a union of partition masks across all configured agents
            //  - 'm_agent_contexts': an agent_ID-keyed map of info return to agent connect() calls

            agent_cfg const & agents = (* m_parent.m_agents);
            ref_data const & ref = (* m_parent.m_ref_data);

            m_liid_limit = agents.liid_limit ();
            m_liid_map = std::make_unique<liid_map_entry []> (m_liid_limit); // filled in the following loop

            agent_contexts contexts { }; // populated below and then moved into 'm_agent_contexts'

            for (auto const & a : agents.agents ())
            {
                agent_ID const & aid = a.first;
                agent_descriptor const & ad = a.second;

                symbol_liid_relation const & aslr = ad.m_symbol_mapping;

                auto const ii = contexts.emplace (aid, agent_context { }).first;
                agent_context & a_ctx = ii->second;

                bitset32_t & a_pix_mask = a_ctx.m_active_partitions;

                for (auto const & sl : aslr.left)
                {
                    std::string const & symbol = sl.first;
                    liid_t const & liid = sl.second;

                    instrument const & i = ref [symbol]; // throws on lookup failure

                    liid_map_entry & lme = m_liid_map [liid];
                    {
                        field<_iid_> (lme) = i.iid ();
                        field<_partition_> (lme) = i.partition ();
                    }

                    a_pix_mask |= (1 << field<_partition_> (lme));

                    LOG_trace1 << "  P" << field<_partition_> (lme) << ": enabling " << print (symbol) << " as liid " << liid << ": " << i;
                    LOG_trace2 << "    " << print (symbol) << " is: " << i;
                }
                LOG_trace1 << "agent " << print (aid) << " uses " << aslr.size () << " instrument(s) and active partition mask " << std::bitset<part_count ()> (a_pix_mask);

                pix_mask |= a_pix_mask;
            }
            m_agent_contexts = std::move (contexts); // note: last use of 'contexts'

            LOG_trace1 << "expecting " << m_agent_contexts.size () << " agent(s)";
            check_nonempty (m_agent_contexts);

            m_request_queues = std::make_unique<ifc_request_queue []> (m_agent_contexts.size ());

            check_nonnull (m_liid_map);
            check_nonzero (pix_mask);

            m_active_partitions = pix_mask;
            LOG_trace1 << "all active partition(s): " << std::bitset<part_count ()> (pix_mask);
        }

        int32_t const capacity = cfg.value ("capacity", io::net::default_tcp_link_capacity ());
        // TODO 'ts_policy' when supported for TCP

        settings const & svr_cfg = cfg.at ("server");
        check_condition (svr_cfg.is_object (), svr_cfg.type ());

        std::string const server = svr_cfg.at ("host");
        uint16_t const port_base = svr_cfg.at ("port");
        check_within (port_base, 64 * 1024);

        // credentials:

        m_account = cfg.at ("account");
        m_credentials = std::make_unique<credentials> (); // note: discarded after 'login()'
        {
            auto const & lp_list = cfg.at ("credentials");
            check_condition (lp_list.is_array (), lp_list.type ());

            for (auto const & lp : lp_list)
            {
                check_eq (lp.size (), 2);
                m_credentials->emplace_back (lp [0].get<std::string> (), lp [1].get<std::string> ());
            }
        }
        check_eq (m_credentials->size (), part_count ());

        // other fixed fields:

        m_fixed_fields = std::make_unique<fixed_fields> (); // TODO populate from 'cfg'
        {
            initialize_msg_templates (cfg);
        }

        // connect all active partitions:

        m_parent.m_threads->attach_to_rcu_callback_thread ();

        while (pix_mask > 0)
        {
            int32_t const pix_bit = (pix_mask & - pix_mask);
            int32_t const pix = int_ops::log2_floor (pix_bit);
            pix_mask ^= pix_bit;

            partition & p = m_partitions [pix];
            {
                p.m_link = io::tcp_link_factory<data_link>::create ("ouch", server, string_cast (port_base + pix), capacity);
            }
        }
    }

    VR_ASSUME_COLD void stop ()
    {
        bitset32_t pix_mask { m_active_partitions };
        while (pix_mask > 0)
        {
            int32_t const pix_bit = (pix_mask & - pix_mask);
            int32_t const pix = int_ops::log2_floor (pix_bit);
            pix_mask ^= pix_bit;

            partition & p = m_partitions [pix];
            {
                p.m_link.reset ();
            }
        }

        m_parent.m_threads->detach_from_rcu_callback_thread ();

        LOG_info << "pd pool capacity at stop time: " << m_pds.capacity () VR_IF_DEBUG (<< " (max size used: " << (m_pool_max_size + 1 ) << ')');
    }

    // post-start logic executed once by each agent (on agent's thread/PU):

    VR_FORCEINLINE execution_link::ifc connect (call_traits<agent_ID>::param aid)
    {
        assert_nonnull (m_request_queues);

        // this is trivially thread-safe because of accessing (almost only) immutable data;
        // the only thing we need to be careful with is more connection requests than
        // what's been configured; simple enough with an atomic counter:

        int32_t const ifc_index = m_agent_connect_count ++; // index into 'm_request_queues'

        if (VR_UNLIKELY (ifc_index > static_cast<int32_t> (m_agent_contexts.size ())))
            throw_x (out_of_bounds, "too many connect attempts");

        LOG_trace1 << "agent " << print (aid) << " connected @ ifc index " << ifc_index;

        // ifc request queues are given out according to (runtime) order of connect()s,
        // everything else is keyed off of 'aid's:

        auto const i = m_agent_contexts.find (aid);
        if (VR_UNLIKELY (i == m_agent_contexts.end ()))
            throw_x (invalid_input, "invalid agent ID " + print (aid));

        agent_context const & a_ctx = i->second;

        return { a_ctx.m_active_partitions, static_cast<uint32_t> (0xA + ifc_index), m_request_queues [ifc_index] };
    }

    // core step logic:

    VR_FORCEINLINE void step () // note: force-inlined
    {
        assert_ne (m_published, m_current); // invariant

        std::array<io::pos_t, part_count ()> pos_min_bound { meta::create_array_fill<io::pos_t, part_count (), std::numeric_limits<io::pos_t>::max ()> () };

        io::pos_t link_pos_flushed [part_count ()]; // TODO init variadically and make const
        for (int32_t pix = 0; pix < part_count (); ++ pix)
        {
            link_pos_flushed [pix] = m_partitions [pix].m_recv_pos_begin;
        }

        // poll all active partition(s) at each step:
        {
            poll_descriptor * const current = m_current;

            bitset32_t active_pix_mask { m_active_partitions };
            bitset32_t update_pix_mask { };

            while (active_pix_mask > 0)
            {
                int32_t const pix_bit = (active_pix_mask & - active_pix_mask); // TODO ASX-148
                int32_t const pix = int_ops::log2_floor (pix_bit);
                active_pix_mask ^= pix_bit;

                partition & p = m_partitions [pix];
                {
                    int32_t link_recv_size { p.m_recv_size };

                    std::pair<addr_const_t, capacity_t> const rc = p.m_link->recv_poll (); // non-blocking read

                    if (rc.second > link_recv_size) // new byte(s) have arrived
                    {
                        // try "consuming" (with the way 'super' is defined it just means walking Soup headers):

                        int32_t const recv_size_adj = (link_recv_size - p.m_recv_available);

                        int32_t available = (rc.second - recv_size_adj);
                        int32_t parsed { }; // more like "parsable by a real OUCH reader"

                        DLOG_trace2 << "[P" << pix << "]: new bytes " << (rc.second - link_recv_size) << ", total available " << available;

                        addr_const_t const data = addr_plus (rc.first, recv_size_adj);

                        while (available >= min_available ()) // enough new byte(s) for a frame header
                        {
                            visit_ctx ctx { };

                            int32_t const rrc = consume (ctx, addr_plus (data, parsed), available);
                            if (rrc < 0)
                                break;

                            available -= rrc;
                            parsed += rrc;
                        }

                        // always remember remember new link size and unconsumed byte count if we've received anything

                        link_recv_size = p.m_recv_size = rc.second;
                        p.m_recv_available = available;

                        // if there is one or more *complete* Soup frames, prepare an RCU update:

                        if (parsed > 0)
                        {
                            // update version for the upcoming RCU writer update:
                            {
                                recv_position & p_current = current->data ()[pix];

                                p_current.m_pos = link_pos_flushed [pix] + parsed + recv_size_adj; // 'm_recv_pos_begin' is updated when allowed by the call_rcu() callback(s)
                                p_current.m_end = addr_plus (data, parsed);
                                p_current.m_ts_local = p.m_link->ts_last_recv ();

                                DLOG_trace2 << "[P" << pix << "]: consumed " << parsed << ", available " << available << ", publishing {pos: " << p_current.m_pos << '}';
                            }

                            update_pix_mask |= pix_bit;
                        }
                    }
                }
            }


            if (update_pix_mask) // publish new data version asap
            {
                poll_descriptor * const published = m_published;

                rcu_assign_pointer (m_published, current); // publish a new descriptor (pointed to by 'current')

                // schedule 'published' to be added to the reclamation stack:

                call_rcu (& published->m_rcu_head, release_poll_descriptor); // [doesn't block]

                // allocate 'm_current' replacement:
                {
                    auto const ar = m_pds.allocate ();

                    poll_descriptor & pd { std::get<0> (ar) };

                    pd.m_parent_reclaim_head = & m_pd_reclaim_head.value ();
                    pd.m_instance = std::get<1> (ar);
                    pd.m_next = -1;

                    // the 'active_pix_mask' loop above is able to update only the partition slots
                    // with new data coming in without losing correctness because we prime the data
                    // array with its prev version:

                    __builtin_memcpy (pd.data (), current->data (), sizeof (poll_descriptor::data_array));

                    m_current = & pd;

                    VR_IF_DEBUG (m_pool_max_size = std::max (m_pool_max_size, pd.m_instance);)
                }
            }
        }

        // maintain 'poll_descriptor' reclaim stack (which will have the side-effect of computing
        // new candidate 'm_recv_pos_begin' values in 'pos_min_bound'):
        {
            // TODO do a non-atomic load of 'm_pd_reclaim_head' first (a la double check locking)

            pd_pool_ref_type i = mc::atomic_xchg (m_pd_reclaim_head.value (), -1); // pop all [wait-free]
            while (true)
            {
                if (i < 0) break;
                DLOG_trace3 << "reclaiming pd slot #" << i;

                poll_descriptor const & pd = m_pds [i];

                // by API contract, for each partition at least one RCU reader has consumed up
                // to 'pd.data ()[pix].m_pos'; hence a guaranteed conservative advance in consumed pos
                // is the min of values that are being reclaimed:

                for (int32_t pix = 0; pix < part_count (); ++ pix) // TODO ASX-148
                {
                    pos_min_bound [pix] = std::min (pos_min_bound [pix], pd.data ()[pix].m_pos);
                }

                pd_pool_ref_type const i_next = pd.m_next;

                m_pds.release (i);
                i = i_next;
            }
        }
        // [note: 'pos_min_bound' can still contain +inf values at this point]

        // poll all connected ifc queues:
        {
            int32_t const agent_count = m_agent_connect_count.load (std::memory_order_relaxed);
            assert_positive (agent_count);

            ifc_request_queue * const request_queues = m_request_queues.get ();

            for (int32_t ifc_index = 0; ifc_index < agent_count; ++ ifc_index)
            {
                auto const e = request_queues [ifc_index].try_dequeue ();
                if (e)
                {
                    xl_request const & req = xl_request_cast (e);
                    DLOG_trace1 << "[ifc " << ifc_index << "] xl_request: " << print (req);

                    xl_req::enum_t const & req_type = field<_type_> (req);

                    // translate 'req' into a message allocated directly in the respective 'm_link's
                    // send buffer and *then* increment 'send_committed'
                    //
                    // the actual I/O will be done outside of this block, after 'e' has been released
                    // and completed the dequeue op:

                    if (VR_LIKELY (req_type != xl_req::start_login)) // frequent case (agent- and partition-specific OUCH request)
                    {
                        switch (req_type)
                        {
                            case xl_req::submit_order:
                            {
                                emit_<ouch::submit_order>  (req);
                            }
                            break;

                            case xl_req::replace_order:
                            {
                                emit_<ouch::replace_order> (req);
                            }
                            break;

                            default: // xl_req::cancel_order
                            {
                                emit_cancel_order (req);
                            }
                            break;

                        } // end of switch
                    }
                    else // 'xl_req::login' (sync point for all agents, translated to Soup login request into all active partitions)
                    {
                        if (++ m_agent_login_count == agent_count)
                        {
                            // note: this link sends login requests, but their processing is done
                            // by all connected agents (they see the same shared login acks/rejects):

                            emit_login ();
                        }
                    }
                }
            } // 'e' is released (dequeue op completes)
        }

        // update 'm_send_committed', 'm_send_flushed' and tend to pending outgoing bytes
        // (including generating heartbeats as needed);
        //
        // update 'm_recv_pos_begin' and 'm_recv_size' fields using 'pos_min_bound' from earlier:
        {
            timestamp_t now_utc { }; // read lazily
            bitset32_t active_pix_mask { m_active_partitions };

            while (active_pix_mask > 0)
            {
                int32_t const pix_bit = (active_pix_mask & - active_pix_mask); // TODO ASX-148
                int32_t const pix = int_ops::log2_floor (pix_bit);
                active_pix_mask ^= pix_bit;

                partition & p = m_partitions [pix];

                // pending outgoing bytes (incl. due to partial sends):

                int32_t const send_pending = (p.m_send_committed - p.m_send_flushed);

                if (send_pending > 0)
                {
                    auto const rc = p.m_link->send_flush (send_pending); // note: 'rc' may be less than 'send_pending'
                    p.m_send_flushed += rc;
                }
                else // heartbeat housekeeping:
                {
                    vr_static_assert (data_link::has_ts_last_send ());

                    now_utc = (now_utc ?  : sys::realtime_utc ()); // TODO parameterize time source (+ see ASX-149)

                    if (VR_UNLIKELY (now_utc >= p.m_link->ts_last_send () + heartbeat_timeout ()))
                    {
                        emit_heartbeat (p);
                    }
                }

                // flushed received bytes:

                io::pos_t const pos_min_bound_pix { pos_min_bound [pix] };
                io::pos_t const link_pos_flushed_pix { link_pos_flushed [pix] };

                // single-branch test of 'link_pos_flushed_pix < pos_min_bound_pix < +inf':

                if (vr_is_in_exclusive_range (pos_min_bound_pix, link_pos_flushed_pix, std::numeric_limits<io::pos_t>::max ()))
                {
                    int32_t const pos_increment = (pos_min_bound_pix - link_pos_flushed_pix);
                    assert_positive (pos_increment);

                    assert_ge (p.m_recv_size, pos_increment);

                    p.m_recv_pos_begin = pos_min_bound_pix;
                    p.m_recv_size -= pos_increment;

                    p.m_link->recv_flush (pos_increment);
                }
            }
        }
    }

    struct partition; // forward

    template<typename MSG>
    struct framed_message
    {
        using schema    = meta::make_schema_t
                        <
                            meta::fdef_<SoupTCP_packet_hdr,     _hdr_>,
                            meta::fdef_<MSG,                    _payload_>
                        >;

        using type      = meta::make_packed_struct_t<schema>;

    }; // end of metafunction


    /**
     * @param req must have the following fields set (fields that are absent in
     *        the actual msg struct will be ignored in any case):
     *
     *                      submit  replace
     *      _liid_              *       *
     *      _otk_               *       *
     *      _new_otk_                   *
     *      _iid_               *
     *      _side_              *
     *      _qty_               *       *
     *      _price_             *       *
     *      _TIF_               *
     *
     * TODO _short_sell_qty_ -> this is possible to figure out at the individual agent level
     * as long as agents trade disjoint sets of symbols
     */
    template<typename REQUEST>
    VR_FORCEINLINE void emit_ (xl_request const & req) // TODO tell compiler 'req' is cl-aligned
    {
        using framed_request    = typename framed_message<REQUEST>::type;

        liid_t const liid = field<_liid_> (req);
        assert_within (liid, m_liid_limit);

        liid_map_entry const & lme = m_liid_map [liid];

        int32_t const & pix = field<_partition_> (lme);
        assert_condition (m_active_partitions & (1 << pix), liid, pix);

        partition & p = m_partitions [pix];

        framed_request const & tpl  = template_for<REQUEST> ();
        constexpr int32_t len   = sizeof (framed_request);

        addr_t const buf = p.m_link->send_allocate (len, false);

        framed_request & frame = * static_cast<framed_request *> (buf);
        __builtin_memcpy (& frame, & tpl, len); // copy template with fixed fields into 'buf'

        REQUEST & msg = field<_payload_> (frame);
        {
            if (has_field<_otk_, REQUEST> ())       // submit/replace
            {
                copy_to_alphanum (field<_otk_> (req), field<_otk_> (msg)); // TODO fast otk copy
            }
            if (has_field<_new_otk_, REQUEST> ())   // replace
            {
                copy_to_alphanum (field<_new_otk_> (req), field<_new_otk_> (msg)); // TODO fast otk copy
            }
            if (has_field<_iid_, REQUEST> ())       // submit
            {
                field<_iid_> (msg) = field<_iid_> (lme);
            }
            if (has_field<_side_, REQUEST> ())      // submit
            {
                field<_side_> (msg) = field<_side_> (req);
            }

            if (has_field<_qty_, REQUEST> ())       // submit/replace
            {
                field<_qty_> (msg) = field<_qty_> (req);
            }
            if (has_field<_price_, REQUEST> ())     // submit/replace
            {
                assert_condition (! data::is_NA (field<_price_> (req)), field<_price_> (req));

                field<_price_> (msg) = price_traits::book_to_wire (field<_price_> (req)); // TODO ASX-151
            }
            if (has_field<_short_sell_qty_, REQUEST> ()) // submit/replace
            {
                field<_short_sell_qty_> (msg) = 0; // TODO set properly (like choosing '_side_', this is up to the agent?)
            }
            if (has_field<_TIF_, REQUEST> ())       // submit
            {
                field<_TIF_> (msg) = field<_TIF_> (req);
            }
        }
        DLOG_trace2 << "send: " << msg;
        p.m_send_committed += len; // note: this just increases commit count, the actual send I/O is coalesced and done elsewhere
    }

    /**
     * @param req must have the following fields set:
     *
     *      _liid_
     *      _otk_
     */
    VR_FORCEINLINE void emit_cancel_order (xl_request const & req) // TODO tell compiler 'req' is cl-aligned
    {
        using framed_request    = typename framed_message<ouch::cancel_order>::type;

        liid_t const liid = field<_liid_> (req);
        assert_within (liid, m_liid_limit);

        liid_map_entry const & lme = m_liid_map [liid];

        int32_t const & pix = field<_partition_> (lme);
        assert_condition (m_active_partitions & (1 << pix), liid, pix);

        partition & p = m_partitions [pix];

        framed_request const & tpl  = m_tpl_cancel_order;
        constexpr int32_t len   = sizeof (framed_request);

        addr_t const buf = p.m_link->send_allocate (len, false);

        framed_request & frame = * static_cast<framed_request *> (buf);
        __builtin_memcpy (& frame, & tpl, len); // copy template with fixed fields into 'buf'

        ouch::cancel_order & msg = field<_payload_> (frame);
        {
            copy_to_alphanum (field<_otk_> (req), field<_otk_> (msg)); // TODO fast otk copy
        }
        DLOG_trace2 << "send: " << msg;
        p.m_send_committed += len; // note: this just increases commit count, the actual send I/O is coalesced and done elsewhere
    }


    void emit_heartbeat (partition & p) // TODO use a template for this, too
    {
        constexpr int32_t len   = sizeof (SoupTCP_packet_hdr);

        addr_t const buf = p.m_link->send_allocate (len, /* special case: no actual low-level protocol payload */false);

        SoupTCP_packet_hdr & hdr = * static_cast<SoupTCP_packet_hdr *> (buf);
        {
            hdr.length () = 1;
            hdr.type () = 'R'; // [SoupTCP] client heartbeat
        }
        p.m_send_committed += len; // note: this just increases commit count, the actual send I/O is coalesced and done elsewhere
    }


    VR_ASSUME_COLD VR_NOINLINE void emit_login ()
    {
        bitset32_t active_pix_mask { m_active_partitions };

        // TODO either stagger partitions a little or use slightly diff timeouts (to stagger heartbeats asap)

        while (active_pix_mask > 0)
        {
            int32_t const pix_bit = (active_pix_mask & - active_pix_mask); // TODO ASX-148
            int32_t const pix = int_ops::log2_floor (pix_bit);
            active_pix_mask ^= pix_bit;

            partition & p = m_partitions [pix];

            auto & sn_expected = m_seqnums [pix];

            std::string const & username = m_credentials->at (pix).first;
            LOG_trace1 << "[P" << pix << "]: logging '" << username << "' with seqnum " << sn_expected << " ...";

            check_nonnull (p.m_link);
            addr_t buf = p.m_link->send_allocate (login_size ()); // blank-filled by default

            SoupTCP_packet_hdr & hdr = * static_cast<SoupTCP_packet_hdr *> (buf);
            {
                hdr.length () = 1 + sizeof (SoupTCP_login_request);
                hdr.type () = 'L'; // [SoupTCP] client login request
            }

            SoupTCP_login_request & msg = * static_cast<SoupTCP_login_request *> (addr_plus (buf, sizeof (SoupTCP_packet_hdr)));
            {
                copy_to_alphanum (username, msg.username ());
                copy_to_alphanum (m_credentials->at (pix).second, msg.password ());
                util::rjust_print_decimal_nonnegative (sn_expected, msg.seqnum ().data (), msg.seqnum ().max_size ());
            }

            p.m_send_committed += login_size (); // note: this just increases commit count, the actual send I/O is coalesced and done elsewhere
        }

        m_credentials.reset ();
    }


    template<typename REQUEST>
    VR_ASSUME_COLD void initialize_ (settings const & cfg, ouch::send_message_type::enum_t const type, typename framed_message<REQUEST>::type & tpl)
    {
        std::memset (& tpl, ' ', sizeof (tpl));

        SoupTCP_packet_hdr & hdr = field<_hdr_> (tpl);
        {
            hdr.length () = 1 + sizeof (REQUEST);
            hdr.type () = 'U'; // [SoupTCP] client unsequenced data
        }

        REQUEST & msg = field<_payload_> (tpl);
        {
            field<_type_> (msg) = type;

            field<_open_close_> (msg) = 0;

            fixed_fields const & ff = (* m_fixed_fields);

            if (has_field<_account_, REQUEST> ())
            {
                check_nonempty (m_account);
                copy_to_alphanum (m_account, field<_account_> (msg));
            }
            // [leave 'customer_info' blank]
            // [leave 'exchange_info' blank]
            if (has_field<_participant_, REQUEST> ())
            {
                field<_participant_> (msg) = ff.m_participant; // char
            }
            if (has_field<_crossing_key_, REQUEST> ())
            {
                field<_crossing_key_> (msg) = ff.m_crossing_key; // int
            }
            if (has_field<_reg_data_, REQUEST> ())
            {
                ouch::reg_data_overlay & rd = field<_reg_data_> (msg);

                field<_participant_capacity_> (rd) = ff.m_participant_capacity; // char
                field<_directed_wholesale_> (rd) = ff.m_directed_wholesale; // char
            }
            if (has_field<_order_type_, REQUEST> ())
            {
                field<_order_type_> (msg) = ord_type::LIMIT;
            }

            if (has_field<_MAQ_, REQUEST> ())
            {
                field<_MAQ_> (msg) = 0;
            }
        }
    }

    VR_ASSUME_COLD void initialize_cancel_order (settings const & cfg, typename framed_message<ouch::cancel_order>::type & tpl)
    {
        std::memset (& tpl, ' ', sizeof (tpl));

        SoupTCP_packet_hdr & hdr = field<_hdr_> (tpl);
        {
            hdr.length () = 1 + sizeof (ouch::cancel_order);
            hdr.type () = 'U'; // [SoupTCP] client unsequenced data
        }

        ouch::cancel_order & msg = field<_payload_> (tpl);
        {
            msg.type () = ouch::message_type<mode::send>::cancel_order;
        }
    }

    VR_ASSUME_COLD void initialize_msg_templates (settings const & cfg)
    {
        check_nonnull (m_fixed_fields);

        initialize_<ouch::submit_order>  (cfg, ouch::send_message_type::submit_order,  m_tpl_submit_order);
        initialize_<ouch::replace_order> (cfg, ouch::send_message_type::replace_order, m_tpl_replace_order);

        initialize_cancel_order (cfg, m_tpl_cancel_order);
    }


    template<typename REQUEST, typename _ = REQUEST>
    VR_FORCEINLINE auto template_for () const
        -> util::enable_if_t<std::is_same<ouch::submit_order, _>::value, typename framed_message<REQUEST>::type const &>
    {
        return m_tpl_submit_order;
    }

    template<typename REQUEST, typename _ = REQUEST>
    VR_FORCEINLINE auto template_for () const
        -> util::enable_if_t<std::is_same<ouch::replace_order, _>::value, typename framed_message<REQUEST>::type const &>
    {
        return m_tpl_replace_order;
    }


    using data_link             = TCP_link<recv<_timestamp_, _tape_>, send<_timestamp_, _tape_>>;

    using pd_pool_ref_type      = poll_descriptor::ref_type;
    using pd_pool_options       = util::pool_options<poll_descriptor, pd_pool_ref_type>::type;
    using pd_pool               = util::object_pool<poll_descriptor,  pd_pool_options>;

    struct partition final
    {
        std::unique_ptr<data_link> m_link { };  // set up in 'start()'
        int64_t   m_send_committed { };
        int64_t   m_send_flushed { };       // tracks 'send_flush()'es
        io::pos_t m_recv_pos_begin { };     // tracks 'recv_flush()'es
        int32_t   m_recv_size { };
        int32_t   m_recv_available { };

    }; // end of nested class

    struct agent_context final // uniquely mapped to an 'agent_ID'
    {
        symbol_liid_relation m_symbol_mapping { };
        bitset32_t m_active_partitions { };

    }; // end of nested class

    using ifc_request_queue     = ifc::request_queue;

    using agent_contexts        = boost::unordered_map<agent_ID, agent_context>; // not perf-critical

    using credentials           = std::vector<std::pair<std::string, std::string>>;

    struct fixed_fields // TODO populate from 'cfg'
    {
        int32_t const m_crossing_key        = 2;
        char const m_participant            = '6';
        char const m_participant_capacity   = 'P';
        char const m_directed_wholesale     = 'N';

    }; // end of class


    execution_link & m_parent;
    poll_descriptor * & m_published;            // aliases 'm_parent::m_published.value()'
    poll_descriptor * m_current { nullptr };    // next 'm_published' (private to this RCU writer)
    std::unique_ptr<ifc_request_queue []> m_request_queues { };     // set in 'start()' to be of 'm_agent_IDs.size()' size; NOTE: only first 'm_agent_count' queues are in actual use
    std::unique_ptr<liid_map_entry [/* liid */]> m_liid_map { };    // set in 'start()'
    std::array<partition, part_count ()> m_partitions { };          // populated in 'start()'
    pd_pool m_pds { };
    bitset32_t m_active_partitions { };         // set in 'start()' [union of partition masks for all configured agents]
    int32_t m_liid_limit { };                   // set in 'start()'
    std::atomic<int32_t> m_agent_connect_count { }; // incremented by 'connect()', immutable afterwards [no need for cl padding]
    int32_t m_agent_login_count { };                // incremented by 'xl_req::login', triggers login into all active partitions for all connected agents
    struct // templates prepared in 'start()', immutable afterwards
    {
        framed_message<ouch::submit_order>::type    m_tpl_submit_order  { };
        framed_message<ouch::replace_order>::type   m_tpl_replace_order { };
        framed_message<ouch::cancel_order>::type    m_tpl_cancel_order  { };
    };
    scope_path const m_cfg_path;
    std::string m_account { };                          // set in 'start()'
    std::unique_ptr<credentials> m_credentials { };     // set in 'start()', discarded after use
    std::unique_ptr<fixed_fields> m_fixed_fields { };   // set in 'start()'
    std::array<int64_t, part_count ()> m_seqnums { 1, 1, 1, 1 }; // TODO
    agent_contexts m_agent_contexts { };                // set in 'start()', immutable afterwards
    VR_IF_DEBUG (int32_t m_pool_max_size { };)

    mc::cache_line_padded_field<pd_pool_ref_type> m_pd_reclaim_head { -1 }; // contended b/w this RCU writer and call_rcu threads (but not with RCU readers)

}; // end of class
//............................................................................
//............................................................................

execution_link::execution_link (scope_path const & cfg_path, std::string const & mock) :
    m_impl { std::make_unique<pimpl> (* this, cfg_path) }
{
    m_impl->initialize (); // slightly tricky initialization order here

    dep (m_config) = "config";
    dep (m_threads) = "threads";
    dep (m_agents) = "agents";
    dep (m_ref_data) = "ref_data";

    if (! mock.empty ()) dep (m_mock) = mock;
}

execution_link::~execution_link ()    = default; // pimpl
//............................................................................

void
execution_link::start ()
{
    m_impl->start ();
}

void
execution_link::stop ()
{
    m_impl->stop ();
}
//............................................................................

execution_link::ifc
execution_link::connect (call_traits<agent_ID>::param aid)
{
    return m_impl->connect (aid);
}
//............................................................................

void
execution_link::step ()
{
    m_impl->step (); // force-inlined
}

} // end of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
