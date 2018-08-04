
#include "vr/market/rt/market_data_feed.h"

#include "vr/io/links/link_factory.h"
#include "vr/io/links/UDP_mcast_link.h"
#include "vr/market/rt/impl/reclamation.h" // release_poll_descriptor()
#include "vr/mc/atomic.h"
#include "vr/mc/mc.h"
#include "vr/mc/thread_pool.h"
#include "vr/rt/cfg/app_cfg.h"
#include "vr/settings.h"
#include "vr/util/object_pools.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
using namespace io;

//............................................................................

vr_static_assert (std::is_standard_layout<recv_reclaim_context>::value);

vr_static_assert (sizeof  (market_data_feed::poll_descriptor) == sys::cpu_info::cache::static_line_size ());
vr_static_assert (alignof (market_data_feed::poll_descriptor) == sys::cpu_info::cache::static_line_size ());

//............................................................................

struct market_data_feed::pimpl final
{
    pimpl (market_data_feed & parent, scope_path const & cfg_path) :
        m_parent { parent },
        m_published { parent.m_published.value () },
        m_cfg_path { cfg_path }
    {
    }

    void initialize ()
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
            VR_IF_DEBUG // part of the init not necessary for correctness
            (
                pd.clear ();
            )

            m_current = & pd;
        }

        assert_ne (m_published, m_current); // invariant
    }

    VR_ASSUME_COLD void start ()
    {
        rt::app_cfg const & config = (* m_parent.m_config);

        settings const & cfg = config.scope (m_cfg_path);
        LOG_trace1 << "using cfg:\n" << print (cfg);
        check_condition (cfg.is_object (), cfg.type ());

        std::string const ifc = cfg.at ("ifc");
        net::mcast_source const ms { cfg.at ("sources").get<std::string> () };
        int32_t const capacity = cfg.value ("capacity", io::net::default_mcast_link_recv_capacity ());
        net::ts_policy::enum_t const tsp = to_enum<net::ts_policy> (cfg.value ("tsp", "hw")); // prod default is 'hw' unless explicitly overridden

        m_parent.m_threads->attach_to_rcu_callback_thread ();

        m_recv_link = io::mcast_link_factory<data_link, data_link::recv_buffer_tag>::create ("itch", ifc,
            { ms }, capacity, { { "tsp", tsp } });

        check_nonnull (m_recv_link);
    }

    VR_ASSUME_COLD void stop ()
    {
        m_recv_link.reset ();

        m_parent.m_threads->detach_from_rcu_callback_thread ();

        LOG_info << "pd pool capacity at stop time: " << m_pds.capacity () VR_IF_DEBUG (<< " (max size used: " << (m_pool_max_size + 1 ) << ')');
    }

    // core step logic:

    VR_FORCEINLINE void step () // note: force-inlined
    {
        assert_ne (m_published, m_current); // invariant

        io::pos_t const link_pos_flushed { m_link_pos_begin };
        int32_t link_size { m_link_size };

        {
            std::pair<addr_const_t, capacity_t> const rc = m_recv_link->recv_poll (); // non-blocking read

            if (rc.second > link_size) // have new byte(s)
            {
                link_size = m_link_size = rc.second; // remember new link size

                poll_descriptor * VR_RESTRICT const published = m_published;

                timestamp_t ts_local;

                // do an RCU writer update on each new datagram:
                {
                    poll_descriptor * VR_RESTRICT const current = m_current;

                    (* current) [0].m_pos = link_pos_flushed + link_size; // 'm_link_pos_begin' is updated when allowed by the call_rcu() callback(s)
                    (* current) [0].m_end = addr_plus (rc.first, link_size);
                    (* current) [0].m_ts_local = ts_local = m_recv_link->ts_last_recv ();

                    rcu_assign_pointer (m_published, current); // publish a new descriptor (pointed to by 'current')
                }

                VR_IF_DEBUG
                (
                    assert_le (m_ts_local_last, ts_local, link_pos_flushed, link_size);
                    m_ts_local_last = ts_local;
                )

                // schedule 'published' to be added to the reclamation stack:

                call_rcu (& published->m_rcu_head, release_poll_descriptor); // [doesn't block]

                // allocate 'm_current' replacement:
                {
                    auto const ar = m_pds.allocate ();

                    poll_descriptor & pd { std::get<0> (ar) };

                    pd.m_parent_reclaim_head = & m_pd_reclaim_head.value ();
                    pd.m_instance = std::get<1> (ar);
                    pd.m_next = -1;
                    VR_IF_DEBUG // part of the init not necessary for correctness
                    (
                        pd.clear ();
                    )

                    m_current = & pd;

                    VR_IF_DEBUG (m_pool_max_size = std::max (m_pool_max_size, pd.m_instance);)
                }
            }
        }

        io::pos_t pos_min_bound = std::numeric_limits<io::pos_t>::max ();

        // maintain the reclaim stack (which will have the side-effect of computing new 'm_link_pos_begin' value):
        {
            pd_pool_ref_type i = mc::atomic_xchg (m_pd_reclaim_head.value (), -1); // pop all [wait-free]
            while (true)
            {
                if (i < 0) break;
                DLOG_trace3 << "reclaiming pd slot #" << i;

                poll_descriptor const & pd = m_pds [i];

                // by API contract, at least one RCU reader has consumed up to 'pd.m_pos';
                // hence a guaranteed conservative advance in consumed pos is the min of values that
                // are being reclaimed:

                pos_min_bound = std::min (pos_min_bound, pd [0].m_pos);
                pd_pool_ref_type const i_next = pd.m_next;

                m_pds.release (i);
                i = i_next;
            }
        }
        // [note: 'pos_min_bound' can still be +inf at this point]

        // single-branch test of 'link_pos_flushed < pos_min_bound < +inf':

        if (vr_is_in_exclusive_range (pos_min_bound, link_pos_flushed, std::numeric_limits<io::pos_t>::max ()))
        {
            // update 'm_link_pos_begin' and 'm_link_size':

            int32_t const pos_increment = (pos_min_bound - link_pos_flushed); // note: this was init'ed such that this diff is safe to narrow to an 'int32_t'
            assert_positive (pos_increment);

            assert_ge (link_size, pos_increment);

            m_link_pos_begin = pos_min_bound;
            m_link_size = link_size - pos_increment;

            m_recv_link->recv_flush (pos_increment);
        }
    }


    using data_link             = UDP_mcast_link<recv<_filter_, _timestamp_, _tape_>>;
    vr_static_assert (data_link::has_recv_filter ());

    using pd_pool_ref_type      = poll_descriptor::ref_type;
    using pd_pool_options       = util::pool_options<poll_descriptor, pd_pool_ref_type>::type;
    using pd_pool               = util::object_pool<poll_descriptor,  pd_pool_options>;


    market_data_feed & m_parent;
    poll_descriptor * & m_published;            // aliases 'm_parent::m_published.value ()'
    std::unique_ptr<data_link> m_recv_link { }; // set up in 'start()'
    poll_descriptor * m_current { nullptr };    // next 'm_published' (private to this RCU writer)
    io::pos_t m_link_pos_begin { };             // tracks 'recv_flush()'es
    pd_pool m_pds { };
    int32_t m_link_size { };
    VR_IF_DEBUG (int32_t m_pool_max_size { };)
    scope_path const m_cfg_path;
    VR_IF_DEBUG (timestamp_t m_ts_local_last { };)

    mc::cache_line_padded_field<pd_pool_ref_type> m_pd_reclaim_head { -1 }; // contended b/w this RCU writer and call_rcu threads (but not with RCU readers)

}; // end of class
//............................................................................
//............................................................................

market_data_feed::market_data_feed (scope_path const & cfg_path) :
    m_impl { std::make_unique<pimpl> (* this, cfg_path) }
{
    m_impl->initialize (); // slightly tricky initialization order here

    dep (m_config) = "config";
    dep (m_threads) = "threads";
}

market_data_feed::~market_data_feed ()    = default; // pimpl
//............................................................................

void
market_data_feed::start ()
{
    m_impl->start ();
}

void
market_data_feed::stop ()
{
    m_impl->stop ();
}
//............................................................................

void
market_data_feed::step ()
{
    m_impl->step (); // force-inlined
}

} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
