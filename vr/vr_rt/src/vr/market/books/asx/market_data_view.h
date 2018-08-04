#pragma once

#include "vr/arg_map.h"
#include "vr/containers/util/chained_scatter_table.h"
#include "vr/market/books/asx/market_data_view_checker.h"
#include "vr/market/books/asx/pool_arenas.h"
#include "vr/market/books/defs.h"
#include "vr/market/ref/asx/ref_data.h"
#include "vr/market/rt/cfg/agent_cfg.h"
#include "vr/market/sources/asx/itch/ITCH_visitor.h"
#include "vr/market/sources/asx/itch/messages_io.h" // print_message()
#include "vr/settings.h"
#include "vr/util/memory.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
namespace ASX
{
/**
 * view limited to one or more instances of the same 'limit_order_book' specialization
 * (which usually mean they represent some subset of instruments at a given venue)
 */
template<typename LIMIT_ORDER_BOOK>
class market_data_view final
{
    private: // ..............................................................

        using this_type     = market_data_view<LIMIT_ORDER_BOOK>;

    public: // ...............................................................

        using book_type     = LIMIT_ORDER_BOOK;

    private: // ..............................................................

        template<typename, typename> friend class market_data_view_checker; // grant private access

        using iid_type          = this_source_traits::iid_type;
        using iid_map_type      = util::chained_scatter_table<iid_type, book_type *, util::identity_hash<iid_type>>;
        using size_type         = typename iid_map_type::size_type;

        vr_static_assert (std::is_signed<size_type>::value); // used for looping below

        /*
         * note: using a nested class here to support ITCH_pipelines (this visitor's
         *       job is just to figure out whether a message is for one of the iids
         *       that are in the view and place the address of the corresponding book
         *       in 'CTX', to be worked on further by subsequent pipeline stages
         *
         * TODO more efficient to do filtering in BE value space
         */
        template<typename CTX>
        class iid_lookup final: public ITCH_visitor<iid_lookup<CTX>>
        {
            private: // ..............................................................

                using super         = ITCH_visitor<iid_lookup<CTX>>;

                vr_static_assert (has_field<_book_, CTX> ());

            public: // ...............................................................

                iid_lookup (arg_map const & args) :
                    super (args),
                    m_parent { args.get<this_type const &> ("view") }
                {
                }

                ~iid_lookup ()
                {
                    VR_IF_DEBUG (LOG_info << "view with " << m_parent.size () << " symbol(s) saw " << m_msg_count << " message(s)";)
                }

                // message visits:

                using super::visit;

                /*
                 * note: this is implemented via a pre-visit to reduce replication of the same table lookup code
                 */
                VR_FORCEINLINE bool visit (pre_message const msg_type, addr_const_t const msg, CTX & ctx) // override
                {
                    itch::message_type::enum_t const mt = static_cast<itch::message_type::enum_t> (static_cast<int32_t> (msg_type));
                    int32_t const iid_offset = itch::message_type::offsetof_iid (mt);

                    if (iid_offset < 0) return true; // non-instrument-specific message type

                    iid_type const iid = (* static_cast<iid_ft const *> (addr_plus (msg, iid_offset)));

                    book_type * const book_ref = m_parent.m_iid_map.get (iid);

                    if (book_ref == nullptr)
                    {
                        field<_book_, CTX> (ctx) = nullptr;
                        return false;
                    }
                    else
                    {
                        field<_book_, CTX> (ctx) = book_ref;

                        VR_IF_DEBUG
                        (
                            ++ m_msg_count;
                            DLOG_trace3 << '[' << m_msg_count << ", iid " << iid << "]: " << itch::print_message (mt, msg); // display what's filtered through, message-specific only (all of traffic can always be traced via "parsing.h")
                        )

                        return true;
                    }
                }

            private: // ..............................................................

                this_type const & m_parent;
                VR_IF_DEBUG (int64_t m_msg_count { };)

        }; // end of nested class

    public: // ...............................................................

        using const_iterator        = typename iid_map_type::const_iterator; // TODO wrap these with classic std::pair value type?
        using iterator              = typename iid_map_type::iterator;

        template<typename CTX>
        using instrument_selector   = iid_lookup<CTX>; // a public connector type to use in ITCH_pipelines and similar


        /**
         * @param args required: "agents"   -> agent_cfg,
         *                       "ref_data" -> ref_data
         */
        market_data_view (arg_map const & args);
        ~market_data_view ();

        // ACCESSORs:

        auto size () const
        {
            return m_iid_map.size ();
        }

        book_type const & operator[] (typename call_traits<iid_type>::param iid) const
        {
            book_type * const book_ref = m_iid_map.get (iid);
            assert_nonnull (book_ref);

            return (* book_ref);
        }

        // iteration:

        const_iterator begin () const
        {
            return m_iid_map.begin ();
        }

        const_iterator end () const
        {
            return m_iid_map.end ();
        }

        // MUTATORs:

        // iteration:

        iterator begin ()
        {
            return m_iid_map.begin ();
        }

        iterator end ()
        {
            return m_iid_map.end ();
        }

        // debug assists:

        VR_ASSUME_COLD void check () const
        {
            market_data_view_checker<this_type>::check_view (* this);
        }

    private: // ..............................................................

        using pool_arena_impl   = impl::market_data_view_pool_arena<typename book_type::traits::pool_arena>;
        using book_storage      = typename std::aligned_storage<sizeof (book_type), alignof (book_type)>::type;


        VR_FORCEINLINE book_type const & at (int32_t const liid) const // used by the checker
        {
            return (* reinterpret_cast<book_type const *> (& m_liid_map [liid]));
        }

        VR_FORCEINLINE book_type & at (int32_t const liid)
        {
            return const_cast<book_type &> (static_cast<this_type const *> (this)->at (liid));
        }


        iid_map_type m_iid_map;
        pool_arena_impl m_pool_arena; // must construct before 'm_liid_map'
        std::unique_ptr<book_storage [/* liid */]> const m_liid_map;

}; // end of class
//............................................................................

template<typename LIMIT_ORDER_BOOK>
market_data_view<LIMIT_ORDER_BOOK>::market_data_view (arg_map const & args) :
    m_iid_map { args.get<agent_cfg &> ("agents").liid_limit () }, // will be populated and then trimmed to fit below
    m_pool_arena { /* TODO scale capacity */ },
    m_liid_map { boost::make_unique_noinit<book_storage []> (args.get<agent_cfg &> ("agents").liid_limit ()) }
{
    agent_cfg const & config = args.get<agent_cfg &> ("agents"); // required
    ref_data const & rd = args.get<ref_data &> ("ref_data"); // required

    // load all symbols that are in the app cfg liid table (but see the TODO in the base class):

    size_type const sz = config.liid_limit ();

    for (liid_t liid = 0; liid < sz; ++ liid)
    {
        std::string const & symbol = config.liid_table ()[liid].m_symbol;
        instrument const & i = rd [symbol];

        LOG_trace2 << "  adding " << print (symbol) << " (P" << i.partition () << ", iid " << i.iid () << ") to the view";

        book_type * const book_addr = & at (liid);

        new (book_addr) book_type { m_pool_arena };
        m_iid_map.put (i.iid (), book_addr);
    }
    m_iid_map.rehash (m_iid_map.size ()); // trim to fit

    check_eq (m_iid_map.size (), sz); // iids are unique by ref data construction
}

template<typename LIMIT_ORDER_BOOK>
market_data_view<LIMIT_ORDER_BOOK>::~market_data_view ()
{
    for (size_type i = m_iid_map.size (); -- i >= 0; )
    {
        util::destruct (at (i));
    }
}

} // end of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
