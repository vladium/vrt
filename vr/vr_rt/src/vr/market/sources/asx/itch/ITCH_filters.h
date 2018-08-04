#pragma once

#include "vr/arg_map.h"
#include "vr/containers/util/chained_scatter_table.h"
#include "vr/fields.h"
#include "vr/market/sources/asx/itch/ITCH_visitor.h"
#include "vr/market/sources/asx/itch/messages_io.h" // print_message()

#include <set>

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
namespace ASX
{
//............................................................................
/**
 * filter for ITCH message types
 */
template<typename CTX>
class ITCH_message_filter: public ITCH_visitor<ITCH_message_filter<CTX> >
{
    private: // ..............................................................

        using super         = ITCH_visitor<ITCH_message_filter<CTX> >;

        using bitset_type   = bitset128_t;
        vr_static_assert (std::get<1> (itch::message_type::enum_range ()) < 8 * sizeof (bitset_type));

    public: // ...............................................................

        ITCH_message_filter (arg_map const & args) :
            m_mf { (args.get<bitset_type> ("messages", -1) | (_one_128 () << itch::message_type::seconds)) } // note: always visit 'seconds' to avoid interfering with origin time tracking
        {
        }


        // overridden visits:

        using super::visit;

        VR_FORCEINLINE bool visit (pre_message const msg_type, addr_const_t const msg, CTX & ctx) // override
        {
            return (m_mf & (_one_128 () << static_cast<int32_t> (msg_type)));
        }

    private: // ..............................................................

        bitset_type const m_mf;

}; // end of class
//............................................................................
/**
 * filter by '_iid_' field
 */
template<typename CTX>
class ITCH_iid_filter: public ITCH_visitor<ITCH_iid_filter<CTX> >
{
    private: // ..............................................................

        using super         = ITCH_visitor<ITCH_iid_filter<CTX> >;

    public: // ...............................................................

        ITCH_iid_filter (arg_map const & args)
        {
            auto const & iids = args.get<std::set<int64_t> > ("instruments", std::set<int64_t> { });

            if (! iids.empty ())
            {
                m_iid_set = std::make_unique<iid_set> (static_cast<typename iid_set::size_type> (iids.size ()));

                for (iid_t const & iid : iids)
                {
                    m_iid_set->put (iid, true); // TODO actual value not used
                }

                m_iid_set->rehash (iids.size ()); // trim to fit
            }
        }


        // overridden visits:

        using super::visit;

        // TODO would be (slightly) more efficient to do filtering in BE value space
        /*
         * note: this is implemented via a pre-visit to reduce replication of the same table lookup code
         */
        VR_FORCEINLINE bool visit (pre_message const msg_type, addr_const_t const msg, CTX & ctx) // override
        {
            iid_set const * const m = m_iid_set.get ();
            if (m == nullptr) return true; // "all"

            itch::message_type::enum_t const mt = static_cast<itch::message_type::enum_t> (static_cast<int32_t> (msg_type));
            int32_t const iid_offset = itch::message_type::offsetof_iid (mt);

            if (iid_offset < 0) return true; // non-instrument-specific message type

            typename iid_set::key_type const iid = (* static_cast<iid_ft const *> (addr_plus (msg, iid_offset)));

            bool const rc = (m->get (iid) != nullptr);
            VR_IF_DEBUG (if (rc) DLOG_trace3 << "iid " << iid << ": " << itch::print_message (mt, msg);) // display what's filtered through, message-specific only (all of traffic can always be traced via "parsing.h")
            return rc;
        }

    private: // ..............................................................

        // TODO support 'V' = void in fast hashtables (ASX-31):
        using iid_set       = util::chained_scatter_table<oid_t, bool, util::identity_hash<oid_t> >;


        std::unique_ptr<iid_set> m_iid_set { }; // TODO parent cls needs to be movable

}; // end of class
//............................................................................
/**
 * filter '[combo_]order_book_dir' by '_product_' field, pass through all other message types
 */
template<typename CTX>
class ITCH_product_filter: public ITCH_visitor<ITCH_product_filter<CTX> >
{
    private: // ..............................................................

        using super         = ITCH_visitor<ITCH_product_filter<CTX> >;

        using bitset_type   = bitset32_t;
        vr_static_assert (std::get<1> (ITCH_product::enum_range ()) < 8 * sizeof (bitset_type));

    public: // ...............................................................

        ITCH_product_filter (arg_map const & args) :
            m_ptf { args.get<bitset_type> ("products", -1) }
        {
        }


        // overridden visits for the only message types that have '_product_':

        using super::visit;

// TODO pass 'seconds' through?
//        VR_ASSUME_HOT bool visit (seconds const & msg, CTX & ctx) // override
//        {
//            return true;
//        }


        VR_ASSUME_HOT bool visit (itch::order_book_dir const & msg, CTX & ctx) // override
        {
            return (m_ptf & (static_cast<bitset_type> (1) << msg.product ()));
        }

        VR_ASSUME_HOT bool visit (itch::combo_order_book_dir const & msg, CTX & ctx) // override
        {
            return visit (static_cast<itch::order_book_dir const &> (msg), ctx);
        }

    private: // ..............................................................

        bitset_type const m_ptf;

}; // end of class

} // end of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
