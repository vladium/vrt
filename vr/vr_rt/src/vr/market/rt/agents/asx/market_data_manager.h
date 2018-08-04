#pragma once

#include "vr/io/net/IP_.h"
#include "vr/io/net/UDP_.h"
#include "vr/io/net/utility.h" // min_size_or_zero
#include "vr/market/books/asx/market_data_listener.h"
#include "vr/market/books/asx/market_data_view.h"
#include "vr/market/books/book_event_context.h"
#include "vr/market/defs.h" // agent_ID, liid_t
#include "vr/market/rt/agents/defs.h"
#include "vr/market/rt/market_data_feed.h"
#include "vr/market/rt/cfg/agent_cfg_fwd.h"
#include "vr/market/sources/asx/itch/ITCH_pipeline.h"
#include "vr/market/sources/asx/itch/Mold_frame_.h"
#include "vr/rt/cfg/app_cfg_fwd.h"
#include "vr/util/datetime.h"
#include "vr/util/logging.h"

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
namespace md
{
using namespace io;
using namespace io::net;


using book_type         = limit_order_book<price_si_t, oid_t, level<_qty_, _order_count_>>;
using view_type         = market_data_view<book_type>;

using visit_ctx         = book_event_context<_book_, _ts_origin_, _packet_index_, _partition_, _ts_local_, _seqnum_,  _dst_port_>;

using selector          = view_type::instrument_selector<visit_ctx>;
using listener          = market_data_listener<this_source (), book_type, visit_ctx>;

using pipeline          = ITCH_pipeline
                        <
                            selector,
                            listener
                        >;

using visitor           = IP_<UDP_<Mold_frame_<pipeline>>>;

struct consume_context final
{
    static constexpr int32_t min_available ()   { return net::min_size_or_zero<visitor>::value (); }

    consume_context (arg_map const & args);

    view_type m_mdv; // TODO needs 'ref_data' and 'instruments' args
    visitor m_visitor;

}; // end of class

} // end of 'md'
} // end of 'impl'
//............................................................................
//............................................................................
/**
 * a building block (base) for an agent
 *
 * this aggregates a market_data_view (a collection of limit_order_book's
 * keyed by their iid's) and structures necessary to interface with a market_data_feed,
 * and implements ITCH visits via an event pipeline that updates limit_order_books in the view
 *
 * @see limit_order_book
 * @see market_data_view
 * @see market_data_feed
 */
class market_data_manager
{
    public: // ...............................................................

        VR_ASSUME_COLD void start (rt::app_cfg const & config, agent_cfg const & agents, ref_data const & rd, agent_ID const & ID);

    protected: // ............................................................

        static constexpr int32_t min_available ()   { return impl::md::consume_context::min_available (); }

        /*
         * poll this manager's market data feed and make the market data view current
         */
        VR_FORCEINLINE void update ()
        {
            assert_nonnull (m_consume_ctx);

            rcu_read_lock (); // [no-op on x86]
            {
                vr_static_assert (market_data_feed::poll_descriptor::width () == 1);

                market_data_feed::poll_descriptor const & pd = m_mdf->poll ();

                int32_t available = (pd [0].m_pos - m_md_ctx.m_pos);
                if (available > 0)
                {
                    DLOG_trace3 << '[' << print_timestamp (pd [0].m_ts_local) << "]: " << available << " byte(s) of ITCH data";

                    VR_IF_DEBUG // track local ts monotonicity
                    (
                        assert_le (m_md_ctx.m_ts_local_last, pd [0].m_ts_local, m_md_ctx.m_pos);
                        m_md_ctx.m_ts_local_last = pd [0].m_ts_local;
                    )

                    addr_const_t const data = addr_plus (pd [0].m_end, -/* ! */available); // start of all new data bytes

                    // although the UDP mcast socket used by 'market_data_feed' will always return a single UDP datagram
                    // when it is read, the feed/link can and will buffer datagrams if for whatever reasons we aren't
                    // consuming them in a timely manner; thus, we must loop here to ensure that we always catch
                    // up when given a chance -- this is part of the design contract for an RCU reader:

                    int32_t consumed { };

                    assert_ge (available, min_available ()); // framing guarantee
                    do
                    {
                        impl::md::visit_ctx ctx { };
                        int32_t const rrc = m_consume_ctx->m_visitor.consume (ctx, addr_plus (data, consumed), available);
                        if (rrc < 0)
                            break;

                        available -= rrc;
                        consumed += rrc;

                        assert_nonnegative (available);
                        assert_le (consumed, (pd [0].m_pos - m_md_ctx.m_pos));
                    }
                    while (VR_UNLIKELY (available >= min_available ()));

                    assert_zero (available);    // RCU reader design contract
                    m_md_ctx.m_pos += consumed; // equivalent to 'm_md_ctx.m_pos = pd [0].m_pos' but updates only local cache line(s)
                }
            }
            rcu_read_unlock (); // [no-op on x86]
        }

        market_data_feed const * m_mdf { }; // [dep]

    private: // ..............................................................

        link_context m_md_ctx { };
        std::unique_ptr<impl::md::consume_context> m_consume_ctx { }; // set by 'start()'

}; // end of class

} // end of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
