#pragma once

#include "vr/containers/util/small_vector.h"
#include "vr/io/net/utility.h" // min_size_or_zero
#include "vr/market/books/asx/execution_view.h"
#include "vr/market/books/asx/execution_listener.h"
#include "vr/market/books/execution_order_book.h"
#include "vr/market/defs.h" // agent_ID, liid_t
#include "vr/market/events/market_event_context.h"
#include "vr/market/rt/agents/defs.h"
#include "vr/market/rt/asx/execution_link.h"
#include "vr/market/rt/asx/utility.h" // order_token_generator
#include "vr/market/rt/cfg/agent_cfg_fwd.h"
#include "vr/market/sources/asx/ouch/OUCH_visitor.h"
#include "vr/market/sources/asx/ouch/Soup_frame_.h"
#include "vr/rt/cfg/app_cfg_fwd.h"
#include "vr/util/datetime.h"
#include "vr/util/logging.h"
#include "vr/util/ops_int.h"

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
namespace ex
{
using namespace io;
using namespace io::net;


using book_type         = execution_order_book<price_si_t, order_token, this_source_traits>;
using view_type         = execution_view<book_type>;

using visit_ctx         = market_event_context<_ts_local_, _ts_origin_, _partition_>;

template<typename DERIVED>
using make_listener     = execution_listener<this_source (), book_type, visit_ctx, DERIVED>;

} // end of 'ex'
} // end of 'impl'
//............................................................................
//............................................................................
/**
 * a building block (base) for an agent
 *
 * this subclasses an execution_view (an indexed container of 'book_order's), inherits
 * OUCH recv visit implementation, and finally adds fields necessary
 * to interface with an execution_link
 *
 * @see execution_order_book
 * @see execution_view
 * @see execution_link
 */
class execution_manager: public impl::ex::view_type, public impl::ex::make_listener<execution_manager> // order of inheritance (construction) is important
{
    private: // ..............................................................

        using view          = impl::ex::view_type;
        using super         = impl::ex::make_listener<execution_manager>;

        vr_static_assert (std::is_same<view::book_type::price_type, price_si_t>::value); // we don't need to rescale API input prices
        vr_static_assert (std::is_same<meta::find_field_def_t<_price_, xl_request>::value_type, price_si_t>::value); // we don't need to rescale API input prices

        using visit_ctx     = impl::ex::visit_ctx;

    public: // ...............................................................

        using liid_vector   = util::small_vector<liid_t, 4>;
        using order_type    = impl::ex::book_type::order_type;

        // ACCESSORs:

        liid_vector const & liids () const;

        // TODO API design: use strong typedefs for int args (e.g. easy to flip 'price' and 'qty' right now)?

        order_type const & submit_limit_order (liid_t const liid, side::enum_t const s, price_si_t const price, qty_t const qty);
        order_type const & submit_IOC_order   (liid_t const liid, side::enum_t const s, price_si_t const price, qty_t const qty);

        order_type const & modify_order (order_token const & otk, price_si_t const price, qty_t const qty);
        void               modify_order (order_type const & o, price_si_t const price, qty_t const qty); // saves an otk lookup

        order_type const & cancel_order (order_token const & otk);
        void               cancel_order (order_type const & o); // saves an otk lookup

        void               erase_order (order_token const & otk);
        void               erase_order (order_type const & o); // saves an otk lookup

    public: // ...............................................................

        // these need to be public for impl reasons:

        void visit_SoupTCP_login (visit_ctx & ctx, SoupTCP_packet_hdr const & soup_hdr, SoupTCP_login_accepted const & msg); // override
        void visit_SoupTCP_login (visit_ctx & ctx, SoupTCP_packet_hdr const & soup_hdr, SoupTCP_login_rejected const & msg); // override

    protected: // ............................................................

        using ex_book_type  = view::book_type;

        VR_ENUM (state,
            (
                start,
                login_barrier,
                running,
                done
            ),
            printable

        ); // end of enum

        static constexpr int32_t part_count = execution_link::poll_descriptor::width ();
        static constexpr int32_t min_available ()   { return io::net::min_size_or_zero<super>::value (); }


        execution_manager (agent_ID const & ID);


        VR_ASSUME_COLD void start (rt::app_cfg const & config, agent_cfg const & agents);


        // ACCESSORs:

        agent_ID const & ID () const
        {
            return m_ID;
        }

        VR_FORCEINLINE state::enum_t const & state () const
        {
            return m_state;
        }

        ex_book_type const & ex_book () const
        {
            return view::book ();
        }

        // MUTATORs:

        /*
         * poll this manager's execution link and make the execution view current
         */
        VR_FORCEINLINE void update ()
        {
            assert_nonnull (m_ex_queue);
            bitset32_t const partition_mask { m_partition_mask };

            rcu_read_lock (); // [no-op on x86]
            {
                using int_ops       = util::ops_int<util::arg_policy<util::zero_arg_policy::ignore, 0>, false>;

                execution_link::poll_descriptor const & pd = m_xl->poll ();

                bitset32_t pix_mask { partition_mask };
                while (pix_mask > 0)
                {
                    int32_t const pix_bit = (pix_mask & - pix_mask); // TODO ASX-148
                    int32_t const pix = int_ops::log2_floor (pix_bit);
                    pix_mask ^= pix_bit;

                    recv_position const & rcvd = pd [pix];
                    link_context & p = m_part_ctx [pix];

                    int32_t available = (rcvd.m_pos - p.m_pos);
                    if (available > 0)
                    {
                        DLOG_trace3 << '[' << print (m_ID) << ", " << print_timestamp (rcvd.m_ts_local) << "]: " << available << " byte(s) of new P" << pix << " data {pos: " << rcvd.m_pos << ", p.m_pos: " << p.m_pos << '}';

                        VR_IF_DEBUG // track local ts monotonicity
                        (
                            assert_le (p.m_ts_local_last, rcvd.m_ts_local, p.m_pos);
                            p.m_ts_local_last = rcvd.m_ts_local;
                        )

                        addr_const_t const data = addr_plus (rcvd.m_end, -/* ! */available);

                        visit_ctx ctx { };
                        {
                            field<_ts_local_> (ctx) = rcvd.m_ts_local;
                            field<_partition_> (ctx) = pix;
                        }

                        // 'execution_link' takes care of Soup TCP framing so here we are guaranteed
                        // to see only a whole number of Soup frames (no partial TCP segments);
                        // we must loop over all of them -- this is part of the design contract for an RCU reader:

                        int32_t consumed { };

                        assert_ge (available, min_available ()); // framing guarantee
                        do
                        {
                            int32_t const rrc = consume (ctx, addr_plus (data, consumed), available);
                            if (rrc < 0)
                                break;

                            available -= rrc;
                            consumed += rrc;
                        }
                        while (VR_UNLIKELY (available >= min_available ()));

                        assert_zero (available);    // RCU reader design contract
                        p.m_pos += consumed;        // equivalent to 'p.m_pos = rcvd.m_pos' but updates only local cache line(s)
                    }
                }
            }
            rcu_read_unlock (); // [no-op on x86]
        }

        order_token otk_gen ()
        {
            return order_token_generator::make_with_prefix<1> (m_otk_counter ++, m_otk_prefix);
        }

        VR_ASSUME_COLD void login_transition ();


        execution_link * m_xl { };          // [dep]

    private: // ..............................................................

        using fsm_state     = ex::fsm_state;

        template<ord_TIF::enum_t TIF>
        VR_FORCEINLINE order_type const & submit_order_impl (liid_t const liid, side::enum_t const s, price_si_t const price, qty_t const qty);
        VR_FORCEINLINE void modify_order_impl (order_type & o, price_si_t const price, qty_t const qty);
        VR_FORCEINLINE void cancel_order_impl (order_type & o);
        VR_FORCEINLINE void erase_order_impl (order_type & o); // this is purely local to the process


        execution_link::ifc::request_queue * m_ex_queue { };
        std::array<link_context, part_count> m_part_ctx { };
        state::enum_t m_state { state::start };
        bitset32_t m_partition_mask { };
        uint32_t m_otk_counter { }; // input into order token gen [non-zero after 'start()']
        uint32_t m_otk_prefix { };  // input into order token gen [assigned by 'm_xl' in 'start()']
        liid_vector m_liids { };

        // fields that aren't read frequently:

        agent_ID const m_ID;
        bitset32_t m_login_pending_mask { };
        std::array<session_context, part_count> m_sessions { };

}; // end of class
//............................................................................

inline execution_manager::liid_vector const &
execution_manager::liids () const
{
    return m_liids;
}
//............................................................................

template<ord_TIF::enum_t TIF>
inline execution_manager::order_type const & // force-inlined
execution_manager::submit_order_impl (liid_t const liid, side::enum_t const s, price_si_t const price, qty_t const qty)
{
    // TODO 1. limit check here

    assert_nonnull (m_ex_queue);

    order_token const otk = otk_gen ();
    order_type & o = view::book ().create_order (liid, otk);

    // set by 'create_order()':
    {
        assert_eq (field<_liid_> (o), liid);
        assert_eq (field<_otk_> (o), otk);

        assert_zero (field<_qty_filled_> (o));
        assert_zero (field<_reject_code_> (o));
        assert_zero (field<_cancel_code_> (o));
    }

    // TODO for consistency with 'modify_order()', do these in the listener? (ruins the design option below, though):

    field<_price_> (o) = price; // note: we asserted statically that no rescaling is needed
    field<_qty_> (o) = qty;

    // TODO the following will be true once/if 'order_type' gets a '_side_' (a field or as a book sub-container):

    // note a design option: 'o' has captured all request parameters and we're about to mark it
    // as being in "pending to enqueue" state; we will spin on xl 'try_enqueue()' since it seems
    // like an unlikely point of contention -- BUT, another option to handle such retries would
    // be to resume with this order on the next 'step()'

    field<_state_> (o).transition (fsm_state::order::created, fsm_state::pending::_);
    {
        while (true) // for now, this spins until 'try_enqueue()' succeeds but that may change
        {
            auto e = m_ex_queue->try_enqueue ();
            if (VR_LIKELY (e))
            {
                xl_request & req = xl_request_cast (e);
                {
                    field<_type_> (req) = xl_req::submit_order;

                    field<_liid_> (req) = liid;
                    field<_otk_> (req) = otk;

                    field<_side_> (req) = (s == side::BID ? ord_side::BUY : ord_side::SELL); // TODO handle ASK properly (locate)
                    field<_price_> (req) = price;
                    field<_qty_> (req) = qty;

                    field<_TIF_> (req) = TIF;
                }
                e.commit ();

                break;
            }
        }
    }
    field<_state_> (o).mark (fsm_state::pending::submit);

    return o;
}
//............................................................................

inline execution_manager::order_type const &
execution_manager::submit_limit_order (liid_t const liid, side::enum_t const s, price_si_t const price, qty_t const qty)
{
    return submit_order_impl<ord_TIF::DAY> (liid, s, price, qty);
}

inline execution_manager::order_type const &
execution_manager::submit_IOC_order (liid_t const liid, side::enum_t const s, price_si_t const price, qty_t const qty)
{
    return submit_order_impl<ord_TIF::IOC> (liid, s, price, qty);
}
//............................................................................

inline void // force-inlined
execution_manager::modify_order_impl (order_type & o, price_si_t const price, qty_t const qty)
{
    assert_nonnull (m_ex_queue);

    // TODO 1. fsm: check whether 'o' state allows a replace = F(type, pending)
    // TODO 2. limit check here

    order_token const new_otk = otk_gen ();

    // add 'new_otk' to the otk map and also to 'o's '_history_':
    // (the latter must be done now to impl a clean undo in case of a cxr reject)

    view::book ().alias_order (o, new_otk); // note: field '_otk_' of 'o' will be updated when the cxr actually succeeds

    {
        while (true) // for now, this spins until 'try_enqueue()' succeeds but that may change
        {
            auto e = m_ex_queue->try_enqueue ();
            if (VR_LIKELY (e))
            {
                xl_request & req = xl_request_cast (e);
                {
                    field<_type_> (req) = xl_req::replace_order;

                    field<_liid_> (req) = field<_liid_> (o);

                    field<_otk_> (req) = field<_otk_> (o); // per venue preference, we will use the most recently known token alias
                    field<_new_otk_> (req) = new_otk;

                    field<_price_> (req) = price;
                    field<_qty_> (req) = qty;

                    // TODO handle ASK/short sell qty properly (locate)
                }
                e.commit ();

                break;
            }
        }
    }
    field<_state_> (o).mark (fsm_state::pending::replace);
}
//............................................................................

inline execution_manager::order_type const &
execution_manager::modify_order (order_token const & otk, price_si_t const price, qty_t const qty)
{
    order_type * const o = view::book ().find_order (otk);
    assert_nonnull (o, otk); // non-optional check (TODO use cf exceptions instead?)

    modify_order_impl (* o, price, qty);

    return (* o);
}

inline void
execution_manager::modify_order (order_type const & o, price_si_t const price, qty_t const qty)
{
    modify_order_impl (* const_cast<order_type *> (& o), price, qty);
}
//............................................................................

inline void // force-inlined
execution_manager::cancel_order_impl (order_type & o)
{
    vr_static_assert (has_field<_liid_, order_type> ());

    assert_nonnull (m_ex_queue);

    // TODO 1. fsm: check whether 'o' state allows a cancel = F(type, pending)
    // TODO 2. limit check here

    {
        while (true) // for now, this spins until 'try_enqueue()' succeeds but that may change
        {
            auto e = m_ex_queue->try_enqueue ();
            if (VR_LIKELY (e))
            {
                xl_request & req = xl_request_cast (e);
                {
                    field<_type_> (req) = xl_req::cancel_order;

                    field<_liid_> (req) = field<_liid_> (o);
                    field<_otk_> (req) = field<_otk_> (o); // per venue preference, we will use the most recently known token alias
                }
                e.commit ();

                break;
            }
        }
    }
    field<_state_> (o).mark (fsm_state::pending::cancel);
}
//............................................................................

inline execution_manager::order_type const &
execution_manager::cancel_order (order_token const & otk)
{
    order_type * const o = view::book ().find_order (otk);
    assert_nonnull (o, otk); // non-optional check (TODO use cf exceptions instead?)

    cancel_order_impl (* o);

    return (* o);
}

inline void
execution_manager::cancel_order (order_type const & o)
{
    cancel_order_impl (* const_cast<order_type *> (& o));
}
//............................................................................

inline void // force-inlined
execution_manager::erase_order_impl (order_type & o)
{
    view::book ().erase_order (o);
}
//............................................................................

inline void
execution_manager::erase_order (order_token const & otk)
{
    order_type * const o = view::book ().find_order (otk);
    assert_nonnull (o, otk); // non-optional check (TODO use cf exceptions instead?)

    erase_order_impl (* o);
}

inline void
execution_manager::erase_order (order_type const & o)
{
    erase_order_impl (* const_cast<order_type *> (& o));
}

} // end of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
