#pragma once

#include "vr/market/sources/asx/ouch/error_codes.h"
#include "vr/market/sources/asx/ouch/OUCH_visitor.h"
#include "vr/market/sources/asx/ouch/Soup_frame_.h"
#include "vr/market/sources/mock/asx/mock_oid_counter.h"
#include "vr/market/sources/mock/asx/mock_order.h"
#include "vr/market/sources/mock/mock_market_event_context.h"
#include "vr/market/sources/mock/mock_response.h"
#include "vr/market/sources/mock/utility.h" // random_range_split
#include "vr/meta/ops_fields.h"
#include "vr/str_hash.h"
#include "vr/util/format.h"
#include "vr/util/logging.h"
#include "vr/util/object_pools.h"

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

constexpr int64_t mock_seqnum_seed ()       { return 100; }

//............................................................................

template<typename TAG, typename MSG>
VR_FORCEINLINE order_token const &
otk_of (MSG const & msg)
{
    vr_static_assert (has_field<TAG, MSG> ());

    return (* reinterpret_cast<order_token const *> (& field<TAG> (msg)[0]));
}
//............................................................................
// OUCH response utilities:

template<typename REQUEST>
void
fill_from (REQUEST const & request, ouch::order_accepted & response)
{
    response.hdr ().type () = ouch::recv_message_type::order_accepted;

    meta::ops_fields<_otk_, _iid_, _side_, _qty_, _price_, _TIF_, _order_type_, _short_sell_qty_, _MAQ_>::copy_all (request, response);

    // TODO reg overlay fields?
}

template<typename REQUEST>
void
fill_from (REQUEST const & request, ouch::order_rejected & response)
{
    response.hdr ().type () = ouch::recv_message_type::order_rejected;

    meta::ops_fields<_otk_>::copy_all (request, response);
}

template<typename REQUEST>
void
fill_from (REQUEST const & request, ouch::order_replaced & response)
{
    response.hdr ().type () = ouch::recv_message_type::order_replaced;

    meta::ops_fields<_new_otk_, _otk_, _qty_, _price_, _short_sell_qty_, _MAQ_>::copy_all (request, response);

    // TODO reg overlay fields?
}

template<typename REQUEST>
void
fill_from (REQUEST const & request, ouch::order_canceled & response)
{
    response.hdr ().type () = ouch::recv_message_type::order_canceled;

    meta::ops_fields<_otk_>::copy_all (request, response);
}

} // end of 'impl'
//............................................................................
//............................................................................
/*
 * this handles:
 *
 *  - 'SoupTCP_login_request' (by always accepting regardless of credentials)
 *
 *  TODO make r_len splitting into "partial write" chunks a compile-time trait
 */
template<typename SERVER, typename CTX, typename DERIVED>
struct mock_ouch_handler_base: public Soup_frame_<io::mode::send, OUCH_visitor<io::mode::send, DERIVED>, DERIVED>
{
    using this_type     = mock_ouch_handler_base<SERVER, CTX, DERIVED>;
    using super         = Soup_frame_<io::mode::send, OUCH_visitor<io::mode::send, DERIVED>, DERIVED>;

    using super::hdr_len;


    mock_ouch_handler_base (SERVER & parent, io::client_connection & cc, arg_map const & args) :
        m_parent { parent },
        m_cc { cc },
        m_rnd { args.get<uint64_t> ("seed") },
        m_eval_time_mean  { args.get<timestamp_t> ("eval_time_mean",  500 * _1_microsecond ()) },
        m_eval_time_range { args.get<timestamp_t> ("eval_time_range", 500 * _1_microsecond ()) }
    {
        check_nonnegative (m_eval_time_mean);
        check_positive (m_eval_time_range);
    }

    // SoupTCP:

    VR_ASSUME_COLD void visit_SoupTCP_login_request (CTX & ctx, SoupTCP_packet_hdr const & packet_hdr, SoupTCP_login_request const & msg) // override
    {
        vr_static_assert (std::is_base_of<this_type, DERIVED>::value);

        switch (m_cc.m_state)
        {
            case io::client_connection::state::login:
            {
                vr_static_assert (has_field<_partition_, CTX> ());
                vr_static_assert (has_field<_ts_local_, CTX> ());

                int32_t const pix = field<_partition_> (ctx);
                timestamp_t const ts_local = field<_ts_local_> (ctx); // this is actually 'now_utc' clock time of the parent server

                LOG_trace1 << "[P" << pix << ", " << print_timestamp (ts_local) << "] client login: " << print (msg);

                timestamp_t const ts_start = ts_local + runif_nonnegative (m_eval_time_mean, m_eval_time_range, m_rnd);

                constexpr int32_t r_len = hdr_len () + sizeof (SoupTCP_login_accepted);
                std::unique_ptr<int8_t []> r_data { std::make_unique<int8_t []> (r_len) };

                __builtin_memset (r_data.get (), ' ', r_len);

                SoupTCP_packet_hdr & hdr { * reinterpret_cast<SoupTCP_packet_hdr *> (r_data.get ()) };
                {
                    hdr.length () = 1 + sizeof (SoupTCP_login_accepted);
                    hdr.type () = 'A';
                }

                SoupTCP_login_accepted & r_msg { * static_cast<SoupTCP_login_accepted *> (addr_plus (r_data.get (), hdr_len ())) };
                {
                    copy_to_alphanum (join_as_name<'_'> (pix, gd::to_iso_string (m_parent.session_date ())), r_msg.session ()); // right-padded with spaces

                    int64_t const seqnum = impl::mock_seqnum_seed () * (1 + pix);

                    util::rjust_print_decimal_nonnegative (seqnum, r_msg.seqnum ().data (), r_msg.seqnum ().max_size ()); // right-padded with spaces
                }

                std::vector<int32_t> len_steps { random_range_split (r_len, 2, m_rnd) }; // TODO randomize 'n'

                std::unique_ptr<mock_response> action { new ouch_response_state_change { m_cc, ts_start, std::move (len_steps), std::move (r_data), io::client_connection::state::serving } }; // note: last use of 'r_data'/'r_msg'

                m_parent.enqueue_action (pix, std::move (action)); // note: last use of 'action'
            }
            break;

            default: throw_x (illegal_state, "unexpected login request in state " + print (m_cc.m_state) + ": " + print (msg));

        } // end of switch
    }


    template<typename RESPONSE>
    static std::unique_ptr<int8_t []> allocate_ ()
    {
        constexpr int32_t r_len = hdr_len () + sizeof (RESPONSE);

        std::unique_ptr<int8_t []> r_data { std::make_unique<int8_t []> (r_len) };

        __builtin_memset (r_data.get (), ' ', r_len);

        SoupTCP_packet_hdr & hdr { * reinterpret_cast<SoupTCP_packet_hdr *> (r_data.get ()) };
        {
            hdr.length () = 1 + sizeof (RESPONSE);
            hdr.type () = 'S';
        }

        return r_data;
    }


    SERVER & m_parent;
    io::client_connection & m_cc;
    uint64_t m_rnd;
    timestamp_t const m_eval_time_mean;
    timestamp_t const m_eval_time_range;

}; // end of class
//............................................................................
/*
 * this is scoped to be in 1:1 association with a client connection ('m_cc')
 *
 * TODO make r_len splitting into "partial write" chunks a compile- or run-time trait
 */
template<typename SERVER, typename CTX>
struct scripted_handler: public mock_ouch_handler_base<SERVER, CTX, /* DERIVED */scripted_handler<SERVER, CTX>>
{
    using super         = mock_ouch_handler_base<SERVER, CTX, scripted_handler<SERVER, CTX>>;

    vr_static_assert (has_field<_mock_scenario_, CTX> ());

    using super::hdr_len;

    using super::super; // inherit constructors

    // OUCH_visitor<send>:

    using super::visit;

    /*
     * submit_order
     */
    bool visit (ouch::submit_order const & msg, CTX & ctx) // override
    {
        vr_static_assert (has_field<_partition_, CTX> ());
        vr_static_assert (has_field<_ts_local_, CTX> ());

        std::string const & scenario = field<_mock_scenario_> (ctx);

        int32_t const pix = field<_partition_> (ctx);
        timestamp_t const ts_local = field<_ts_local_> (ctx); // this is actually 'now_utc' clock time of the parent server

        LOG_trace1 <<  "[P" << pix << ", " << print_timestamp (ts_local) << "] client request " << print (msg);

        timestamp_t const ts_start = ts_local + runif_nonnegative (m_eval_time_mean, m_eval_time_range, m_rnd);
        std::unique_ptr<mock_response> action { };

        order_token const & otk = impl::otk_of<_otk_> (msg);
        check_condition (! m_otk_map.count (otk), otk);

        switch (str_hash_32 (scenario))
        {
            default: // TODO until scripting works
            case "accept"_hash:
            {
                // enqueue 'order_accepted' response:

                using response_type = ouch::order_accepted;

                oid_t const oid = m_oid_gen (msg.iid (), ord_side::to_side (msg.side ()));
                assert_condition (! m_oid_map.count (oid), oid, otk);

                // allocate new mock order and add it to the maps:

                auto const ar = m_order_pool.allocate ();
                mock_order & mo = std::get<0> (ar);
                {
                    mo.m_instance = std::get<1> (ar);
                }
                m_otk_map.emplace (otk, & mo);
                m_oid_map.emplace (oid, & mo);
                {
                    mo.m_oid = oid;
                    mo.m_iid = msg.iid ();
                    mo.m_state = ord_state::LIVE; // TODO need some control over this
                    mo.m_order_type = msg.order_type ();

                    mo.m_side = msg.side ();
                    mo.m_price = msg.price ();
                    mo.m_qty = msg.qty ();
                }

                constexpr int32_t r_len = hdr_len () + sizeof (response_type);
                std::unique_ptr<int8_t []> r_data { super::template allocate_<response_type> () };

                response_type & r_msg { * static_cast<response_type *> (addr_plus (r_data.get (), hdr_len ())) };
                {
                    impl::fill_from (msg, r_msg);

                    r_msg.hdr ().ts () = ts_start;

                    r_msg.oid () = oid;
                    r_msg.state () = mo.m_state;
                }

                std::vector<int32_t> len_steps { random_range_split (r_len, 2, m_rnd) }; // TODO randomize 'n'

                action.reset (new ouch_response { m_cc, ts_start, std::move (len_steps), std::move (r_data) }); // note: last use of 'r_data'/'r_msg'
            }
            break;

            case "reject"_hash:
            {
                // enqueue 'order_rejected' response:

                using response_type = ouch::order_rejected;

                constexpr int32_t r_len = hdr_len () + sizeof (response_type);
                std::unique_ptr<int8_t []> r_data { super::template allocate_<response_type> () };

                response_type & r_msg { * static_cast<response_type *> (addr_plus (r_data.get (), hdr_len ())) };
                {
                    impl::fill_from (msg, r_msg);

                    r_msg.hdr ().ts () = ts_start;

                    r_msg.reject_code () = fixnetix_reject::REJ_INVALID_TICK_SIZE; // TODO some control over this
                }

                std::vector<int32_t> len_steps { random_range_split (r_len, 2, m_rnd) }; // TODO randomize 'n'

                action.reset (new ouch_response { m_cc, ts_start, std::move (len_steps), std::move (r_data) }); // note: last use of 'r_data'/'r_msg'
            }
            break;

            // TODO default: throw_x (illegal_state, "unexpected scenario step " + print (scenario));

        } // end of switch

        assert_nonnull (action);
        m_parent.enqueue_action (pix, std::move (action)); // note: last use of 'action'

        return true;
    }

    /*
     * replace_order
     */
    bool visit (ouch::replace_order const & msg, CTX & ctx) // override
    {
        vr_static_assert (has_field<_partition_, CTX> ());
        vr_static_assert (has_field<_ts_local_, CTX> ());

        int32_t const pix = field<_partition_> (ctx);
        timestamp_t const ts_local = field<_ts_local_> (ctx); // this is actually 'now_utc' clock time of the parent server

        LOG_trace1 <<  "[P" << pix << ", " << print_timestamp (ts_local) << "] client request " << print (msg);

        timestamp_t const ts_start = ts_local + runif_nonnegative (m_eval_time_mean, m_eval_time_range, m_rnd);
        std::unique_ptr<mock_response> action { };

        order_token const & otk = impl::otk_of<_otk_> (msg);

        auto const i = m_otk_map.find (otk);
        check_condition (i != m_otk_map.end (), otk);

        assert_nonnull (i->second);
        mock_order & mo = (* i->second);

        mo.m_price = msg.price ();
        mo.m_qty = msg.qty ();
        // TODO short_sell_qty and MAQ ?

        order_token const & new_otk = impl::otk_of<_new_otk_> (msg);
        m_otk_map.emplace (new_otk, & mo); // alias
         {
            // enqueue 'order_replaced' response:

            using response_type = ouch::order_replaced;

            constexpr int32_t r_len = hdr_len () + sizeof (response_type);
            std::unique_ptr<int8_t []> r_data { super::template allocate_<response_type> () };

            response_type & r_msg { * static_cast<response_type *> (addr_plus (r_data.get (), hdr_len ())) };
            {
                impl::fill_from (msg, r_msg);

                r_msg.hdr ().ts () = ts_start;

                r_msg.iid () = mo.m_iid;
                r_msg.side () = mo.m_side;
                r_msg.oid () = mo.m_oid;

                r_msg.state () = mo.m_state;
                r_msg.order_type () = mo.m_order_type;
            }

            std::vector<int32_t> len_steps { random_range_split (r_len, 2, m_rnd) }; // TODO randomize 'n'

            // TODO need to GC this order eventually

            action.reset (new ouch_response { m_cc, ts_start, std::move (len_steps), std::move (r_data) }); // note: last use of 'r_data'/'r_msg'
        }

        assert_nonnull (action);
        m_parent.enqueue_action (pix, std::move (action)); // note: last use of 'action'

        return true;
    }

    /*
     * cancel_order
     */
    bool visit (ouch::cancel_order const & msg, CTX & ctx) // override
    {
        vr_static_assert (has_field<_partition_, CTX> ());
        vr_static_assert (has_field<_ts_local_, CTX> ());

        int32_t const pix = field<_partition_> (ctx);
        timestamp_t const ts_local = field<_ts_local_> (ctx); // this is actually 'now_utc' clock time of the parent server

        LOG_trace1 <<  "[P" << pix << ", " << print_timestamp (ts_local) << "] client request " << print (msg);

        timestamp_t const ts_start = ts_local + runif_nonnegative (m_eval_time_mean, m_eval_time_range, m_rnd);
        std::unique_ptr<mock_response> action { };

        order_token const & otk = impl::otk_of<_otk_> (msg);

        auto const i = m_otk_map.find (otk);
        check_condition (i != m_otk_map.end (), otk);

        assert_nonnull (i->second);
        mock_order & mo = (* i->second);

        {
            // enqueue 'order_rejected' response:

            using response_type = ouch::order_canceled;

            constexpr int32_t r_len = hdr_len () + sizeof (response_type);
            std::unique_ptr<int8_t []> r_data { super::template allocate_<response_type> () };

            response_type & r_msg { * static_cast<response_type *> (addr_plus (r_data.get (), hdr_len ())) };
            {
                impl::fill_from (msg, r_msg);

                r_msg.hdr ().ts () = ts_start;

                r_msg.iid () = mo.m_iid;
                r_msg.side () = mo.m_side;
                r_msg.oid () = mo.m_oid;

                r_msg.cancel_code () = cancel_reason::REQUESTED_BY_USER; // TODO need some control over this
            }

            std::vector<int32_t> len_steps { random_range_split (r_len, 2, m_rnd) }; // TODO randomize 'n'

            // TODO need to GC this order eventually

            action.reset (new ouch_response { m_cc, ts_start, std::move (len_steps), std::move (r_data) }); // note: last use of 'r_data'/'r_msg'
        }

        assert_nonnull (action);
        m_parent.enqueue_action (pix, std::move (action)); // note: last use of 'action'

        return true;
    }


    using oid_count_policy  = mock_oid_counter<mock_oid_scheme::adversarial>;

    using order_pool        = util::object_pool<mock_order>;
    using otk_order_map     = boost::unordered_map<order_token, mock_order *>; // values ref (stable) 'm_order_pool' slots
    using oid_order_map     = boost::unordered_map<oid_t, mock_order *>;


    using super::m_parent;
    using super::m_cc;
    using super::m_rnd;
    using super::m_eval_time_mean;
    using super::m_eval_time_range;
    oid_count_policy m_oid_gen { 0x607A7D810000005A };
    order_pool m_order_pool { 128 };
    otk_order_map m_otk_map { };
    oid_order_map m_oid_map { };

}; // end of class

} // end of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
