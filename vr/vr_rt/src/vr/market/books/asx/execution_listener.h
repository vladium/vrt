#pragma once

#include "vr/fields.h"
#include "vr/market/rt/agents/exceptions.h" // agent_runtime_exception
#include "vr/market/books/execution_listener.h"
#include "vr/market/books/execution_order_book.h"
#include "vr/market/sources/asx/ouch/OUCH_visitor.h"
#include "vr/market/sources/asx/ouch/Soup_frame_.h"
#include "vr/util/logging.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
//............................................................................

VR_DEFINE_EXCEPTION (order_state_exception, agent_runtime_exception);

//............................................................................
/**
 * @note 'DERIVED' handles Soup login ack/reject
 */
// TODO a trait for handling policy for error (fatal and/or 'not my oid', etc)
//
template<typename EXECUTION_BOOK, typename CTX, typename DERIVED> // specialization for ASX
class execution_listener<source::ASX, EXECUTION_BOOK, CTX, DERIVED>: public ASX::Soup_frame_<io::mode::recv,
                                                                                             ASX::OUCH_visitor<io::mode::recv, execution_listener<source::ASX, EXECUTION_BOOK, CTX, DERIVED>>,
                                                                                             DERIVED>
{
    private: // ..............................................................

        // this is "slightly" hairy because we don't (yet) have an OUCH processing pipeline:

        using this_type     = execution_listener<source::ASX, EXECUTION_BOOK, CTX, DERIVED>;

        using super         = ASX::Soup_frame_<io::mode::recv,
                                               ASX::OUCH_visitor<io::mode::recv, this_type>, // we handle VR_MARKET_OUCH_RECV_MESSAGE_SEQ
                                               DERIVED>;                                     // 'DERIVED' can handle rest of Soup (mainly, login ack/reject)

        using price_traits  = typename source_traits<source::ASX>::price_traits<typename EXECUTION_BOOK::price_type>;

    public: // ...............................................................

        using book_type             = EXECUTION_BOOK;

        using order_type            = typename book_type::order_type;
        using otk_type              = typename book_type::otk_type;
        using oid_type              = typename book_type::oid_type;

        using otk_storage_type      = typename book_type::otk_storage_type;
        using fast_order_ref_type   = typename book_type::fast_order_ref_type;


        execution_listener (EXECUTION_BOOK & book) :
            m_book { book }
        {
        }

        // these need to be public for design reasons:

        // OUCH recv overrides:

        using super::visit;

        /*
         * order_accepted
         */
        VR_ASSUME_HOT bool visit (ASX::ouch::order_accepted const & msg, CTX & ctx) // override
        {
            using namespace ASX;

            DLOG_trace2 << "visit: " << msg;

            otk_type const & otk = otk_of<_otk_> (msg); // lookup by otk is the only option
            order_type * const o_ptr = m_book.m_otk_map.get (static_cast<otk_storage_type> (otk));

            if (VR_LIKELY (o_ptr != nullptr))
            {
                order_type & o = (* o_ptr);

                VR_IF_DEBUG
                (
                    auto const os = field<_state_> (o); // copy
                )

                switch (static_cast<fsm_bitset_t> (field<_state_> (o)))
                {
                    case (ex::fsm_state::order::created | ex::fsm_state::pending::submit): // expected state
                    {
                        oid_type const oid = msg.oid ();

                        field<_oid_> (o) = oid;
                        m_book.m_oid_map.put (oid, & o); // record the oid mapping

                        switch (field<_state_> (msg))
                        {
                            case ord_state::LIVE:
                            {
                                // accepted and live:

                                // check other fields against our expectations:
                                // TODO an option to always this at runtime?

                                assert_eq (price_traits::wire_to_book (msg.price ()), o.price (), msg.price ());

                                assert_eq (field<_order_type_> (msg), ord_type::LIMIT);
                                assert_zero (field<_MAQ_> (msg));

                                field<_state_> (o).transition (ex::fsm_state::order::live, ex::fsm_state::pending::_);
                            }
                            break;

                            case ord_state::NOT_ON_BOOK:
                            {
                                // "accepted" but not on book implies there will be more
                                // messages following up (executions and/or cancellation):

                                field<_state_> (o).transition (ex::fsm_state::order::done, ex::fsm_state::pending::finalize);
                            }
                            break;

                        } // end of switch
                    }
                    break;

                    default:
                    {
                        o.fsm_error_transition (__LINE__);
                    }

                } // end of switch

                VR_IF_DEBUG
                (
                    if (field<_state_> (o) != os)
                    {
                        DLOG_trace1 << print (otk) << ": transition " << os << " -> " << field<_state_> (o);
                        DLOG_trace2 << "  " << print (o);
                    }
                )
            }
            else // not an error since can be another agent's order
            {
                DLOG_trace1 << "otk " << print (otk) << " not in the book, assuming belongs to another agent: " << msg;
            }

            return super::visit (msg, ctx); // [chain]
        }

        /*
         * order_rejected (note: hinted cold)
         */
        VR_ASSUME_COLD bool visit (ASX::ouch::order_rejected const & msg, CTX & ctx) // override
        {
            using namespace ASX;

            DLOG_trace2 << "visit: " << msg;

            otk_type const & otk = otk_of<_otk_> (msg); // lookup by otk is the only option
            order_type * const o_ptr = m_book.m_otk_map.get (static_cast<otk_storage_type> (otk));

            if (VR_LIKELY (o_ptr != nullptr))
            {
                order_type & o = (* o_ptr);

                VR_IF_DEBUG (auto const os = field<_state_> (o);) // copy

                field<_reject_code_> (o) = msg.reject_code ();

                switch (static_cast<fsm_bitset_t> (field<_state_> (o)))
                {
                    case (ex::fsm_state::order::created | ex::fsm_state::pending::submit):
                    {
                        field<_state_> (o).transition (ex::fsm_state::order::done, ex::fsm_state::pending::_);
                    }
                    break;

                    // TODO need tests for these + prod traces:

                    case (ex::fsm_state::order::live | ex::fsm_state::pending::replace):
                    {
                        // the order is still live, so just clear pending bits:

                        // [there is no way to attribute this reject to a particular
                        //  pending op; the expectation is not to have more than one]

                        field<_state_> (o).clear (ex::fsm_state::pending::replace);

                    }
                    break;

                    case (ex::fsm_state::order::live | ex::fsm_state::pending::cancel):
                    {
                        // the order is still live, so just clear pending bits:

                        // [there is no way to attribute this reject to a particular
                        //  pending op; the expectation is not to have more than one]

                        field<_state_> (o).clear (ex::fsm_state::pending::cancel);
                    }
                    break;

                    default:
                    {
                        o.fsm_error_transition (__LINE__);
                    }

                } // end of switch

                VR_IF_DEBUG
                (
                    if (field<_state_> (o) != os)
                    {
                        DLOG_trace1 << print (otk) << ": transition " << os << " -> " << field<_state_> (o);
                        DLOG_trace2 << "  " << print (o);
                    }
                )
            }
            else // not an error since can be another agent's order
            {
                DLOG_trace1 << "otk " << print (otk) << " not in the book, assuming belongs to another agent: " << msg;
            }

            return super::visit (msg, ctx); // [chain]
        }

        /*
         * order_replaced
         */
        VR_ASSUME_HOT bool visit (ASX::ouch::order_replaced const & msg, CTX & ctx) // override
        {
            using namespace ASX;

            DLOG_trace2 << "visit: " << msg;

            oid_type const oid = field<_oid_> (msg); // prefer to look up by oid
            order_type * const o_ptr = m_book.m_oid_map.get (oid);

            if (VR_LIKELY (o_ptr != nullptr))
            {
                order_type & o = (* o_ptr);

                VR_IF_DEBUG (auto const os = field<_state_> (o);) // copy

                switch (field<_state_> (o).order_state ())
                {
                    case (ex::fsm_state::order::live):
                    {
                        switch (field<_state_> (msg))
                        {
                            case ord_state::LIVE:
                            {
                                // replaced and live:

                                field<_price_> (o) = price_traits::wire_to_book (msg.price ());
                                field<_qty_> (o) = msg.qty ();

                                // check other fields against our expectations:
                                // TODO an option to always this at runtime?

                                assert_eq (field<_order_type_> (msg), ord_type::LIMIT);
                                assert_zero (field<_MAQ_> (msg));

                                field<_state_> (o).transition (ex::fsm_state::order::live, ex::fsm_state::pending::_);
                            }
                            break;

                            case ord_state::NOT_ON_BOOK:
                            {
                                // can this ever happen?

                                // "replaced" but not on book implies there will be more
                                // messages following up (executions and/or cancellation):

                                field<_state_> (o).transition (ex::fsm_state::order::done, ex::fsm_state::pending::finalize);
                            }
                            break;

                        } // end of switch
                    }
                    break;

                    default:
                    {
                        o.fsm_error_transition (__LINE__);
                    }

                } // end of switch

                VR_IF_DEBUG
                (
                    if (field<_state_> (o) != os)
                    {
                        DLOG_trace1 << print (oid) << ": transition " << os << " -> " << field<_state_> (o);
                        DLOG_trace2 << "  " << print (o);
                    }
                )
            }
            else // not an error since can be another agent's order
            {
                DLOG_trace1 << "otk " << print (oid) << " not in the book, assuming belongs to another agent: " << msg;
            }

            return super::visit (msg, ctx); // [chain]
        }

        /*
         * order_canceled
         */
        VR_ASSUME_HOT bool visit (ASX::ouch::order_canceled const & msg, CTX & ctx) // override
        {
            using namespace ASX;

            DLOG_trace2 << "visit: " << msg;

            oid_type const oid = field<_oid_> (msg); // prefer to look up by oid
            order_type * const o_ptr = m_book.m_oid_map.get (oid);

            if (VR_LIKELY (o_ptr != nullptr))
            {
                order_type & o = (* o_ptr);

                VR_IF_DEBUG (auto const os = field<_state_> (o);) // copy

                switch (field<_state_> (o).order_state ())
                {
                    case (ex::fsm_state::order::live):
                    {
                        // note: at the moment, not checking for 'pending::cancel'
                        // and/or reconciling it with 'msg.cancel_code()'

                        field<_state_> (o).transition (ex::fsm_state::order::done, ex::fsm_state::pending::_);

                        field<_cancel_code_> (o) = msg.cancel_code ();

                        // note: the order remains a valid book entry (i.e. in both oid and otk maps (incl.
                        // the otk history) and in the order pool) until higher-level code invokes
                        // book's 'erase_order()'
                    }
                    break;

                    default:
                    {
                        o.fsm_error_transition (__LINE__);
                    }

                } // end of switch

                VR_IF_DEBUG
                (
                    if (field<_state_> (o) != os)
                    {
                        DLOG_trace1 << print (oid) << ": transition " << os << " -> " << field<_state_> (o);
                        DLOG_trace2 << "  " << print (o);
                    }
                )
            }
            else // not an error since can be another agent's order
            {
                DLOG_trace1 << "otk " << print (oid) << " not in the book, assuming belongs to another agent: " << msg;
            }

            return super::visit (msg, ctx); // [chain]
        }

        /*
         * order_execution
         */
        VR_ASSUME_HOT bool visit (ASX::ouch::order_execution const & msg, CTX & ctx) // override
        {
            using namespace ASX;

            DLOG_trace2 << "visit: " << msg;

            // TODO don't forget to transition to DONE if gets fully filled

            return super::visit (msg, ctx); // [chain]
        }

    private: // ..............................................................

        using fsm_bitset_t  = typename meta::find_field_def_t<_state_, order_type>::value_type::bitset_type;

        /*
         * taking perf advantage of our token scheme:
         *
         * TODO handle the blank token cancel special case
         */
        template<typename TAG, typename MSG>
        static VR_FORCEINLINE otk_type const & otk_of (MSG const & msg)
        {
            vr_static_assert (has_field<TAG, MSG> ());

            return (* reinterpret_cast<otk_type const *> (& field<TAG> (msg)[0]));
        }

        EXECUTION_BOOK & m_book;

}; // end of class

} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
