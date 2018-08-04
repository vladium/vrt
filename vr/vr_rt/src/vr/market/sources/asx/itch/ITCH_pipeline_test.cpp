
#include "vr/io/cap/cap_reader.h"
#include "vr/io/net/IP_.h"
#include "vr/io/net/pcap_.h"
#include "vr/io/net/UDP_.h"
#include "vr/io/stream_factory.h"
#include "vr/market/books/asx/market_data_listener.h"
#include "vr/market/books/book_event_context.h"
#include "vr/market/sources/asx/itch/ITCH_pipeline.h"
#include "vr/market/sources/asx/market_data.h"

#include "vr/test/files.h"
#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
namespace ASX
{
using namespace io;
using namespace io::net;
using namespace market;

//............................................................................
//............................................................................
namespace
{

struct visit_marker final
{
    visit_marker (addr_const_t const visitor, char const visitor_ID, itch::message_type::enum_t const msg_type) :
        m_visitor { visitor },
        m_msg_type { msg_type },
        m_visitor_ID { visitor_ID }
    {
    }

    addr_const_t const m_visitor;
    itch::message_type::enum_t const m_msg_type;
    char const m_visitor_ID;

}; // end of class

using visit_stack   = std::vector<visit_marker>;

//............................................................................

template<char ID, typename CTX>
class hook_visitor: public ITCH_visitor<hook_visitor<ID, CTX> >
{
    private: // ..............................................................

        using super         = ITCH_visitor<hook_visitor<ID, CTX> >;

    public: // ...............................................................

        hook_visitor (arg_map const & args) :
            m_stack { args.get<visit_stack *> ("stack") },
            m_rnd { args.get<int32_t> (join_as_name ("seed", ID)) }
        {
        }

        ~hook_visitor ()
        {
            LOG_info << print (ID) << " visits: add " << print (m_order_add_counts) << ", delete " << print (m_order_delete_counts);
        }

        // overridable visits:

        using super::visit;

        // pre_/post_message:

        bool visit (pre_message const msg_type, addr_const_t const msg, CTX & ctx) // override
        {
            itch::message_type::enum_t const mt = static_cast<itch::message_type::enum_t> (static_cast<int32_t> (msg_type));

            m_visited = false; // clear flag
            bool const rc = (test::next_random (m_rnd) > 0);
            DLOG_trace2 << "  pre-visit (" << ID << ", " << mt << "): " << rc;

            return rc;
        }

        void visit (post_message const msg_type, CTX & ctx) // override
        {
            DLOG_trace2 << " post-visit (" << ID << ')';

            if (m_visited) // one of the two overridden message visits
            {
                itch::message_type::enum_t const mt = static_cast<itch::message_type::enum_t> (static_cast<int32_t> (msg_type));

                switch (mt)
                {
                    case itch::message_type::order_add: ++ m_order_add_counts [1]; break;
                    case itch::message_type::order_delete: ++ m_order_delete_counts [1]; break;

                    default: throw_x (illegal_state, "unexpected message type " + print (mt));

                } // end of switch

                check_nonempty (* m_stack);

                // validate that we're post-visiting what we've visited:

                auto const & marker = m_stack->back ();

                check_eq (marker.m_visitor_ID, ID); // it's us
                check_eq (marker.m_visitor, static_cast<addr_const_t> (this)); // it's us
                check_eq (marker.m_msg_type, mt); // it's a correct msg type (TODO store msg addr when/if supported)

                m_stack->pop_back (); // pop

                // similarly, validate that visit and post-visit are always invoked in pairs:

                check_eq (m_order_add_counts [0], m_order_add_counts [1], ID);
                check_eq (m_order_delete_counts [0], m_order_delete_counts [1], ID);
            }
        }


        // some select message type visits:

        bool visit (itch::order_add const & msg, CTX & ctx) // override
        {
            m_visited = true;
            m_stack->emplace_back (this, ID, itch::message_type::order_add); // push

            ++ m_order_add_counts [0];

            bool const rc = (test::next_random (m_rnd) > 0);
            DLOG_trace2 << "    visit (" << ID << ", order_add): " << rc;

            return rc;
        }
        bool visit (itch::order_delete const & msg, CTX & ctx) // override
        {
            m_visited = true;
            m_stack->emplace_back (this, ID, itch::message_type::order_delete); // push

            ++ m_order_delete_counts [0];

            bool const rc = (test::next_random (m_rnd) > 0);
            DLOG_trace2 << "    visit (" << ID << ", order_delete): " << rc;

            return rc;
        }

    private: // ..............................................................

        visit_stack * const m_stack;
        std::array<int64_t, 2> m_order_add_counts { { 0, 0 } };     // visit/post-visit
        std::array<int64_t, 2> m_order_delete_counts { { 0, 0 } };  // visit/post-visit
        int32_t m_rnd;
        bool m_visited { false };

}; // end of class

} // end of anonymous
//............................................................................
//............................................................................
/*
 * pump a larg-ish data sample through a pipeline of 3 visitors with randomized
 * responses in [pre-] message visits
 */
// TODO
// - test pre_/post_packet
TEST (ASX_ITCH_pipeline, visit_nesting)
{
    using reader            = cap_reader;

    using visit_ctx         = book_event_context<_ts_origin_, _packet_index_, _partition_, _ts_local_, _ts_local_delta_, _seqnum_, _dst_port_>;

    using pipeline          = ITCH_pipeline
                            <
                                hook_visitor<'A', visit_ctx>,
                                hook_visitor<'B', visit_ctx>,
                                hook_visitor<'C', visit_ctx>
                            >;

    using visitor           = pcap_<IP_<UDP_<Mold_frame_<pipeline>>>>;

    // TODO use a short sample (checked in?)

    fs::path const test_input = test::find_capture (source::ASX, "<"_rop, util::current_date_in ("Australia/Sydney"));
    LOG_info << "using test data in " << print (test_input);

    std::unique_ptr<std::istream> const in = stream_factory::open_input (test_input / "mcast.recv.p1p2.pcap.zst");

    int32_t seed { test::env::random_seed<int32_t> () };

    visit_stack s { };
    visitor v { { { "stack", & s }, { "seed.A", seed }, { "seed.B", seed * 3 }, { "seed.C", seed * 7 } } }; // make sure rnd series are different

    visit_ctx ctx { };
    reader r { * in, cap_format::pcap };

    r.evaluate (ctx, v);

    EXPECT_TRUE (s.empty ());
}

} // end of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
