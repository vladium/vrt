
#include "vr/io/sql/sql_connection_factory.h"

#include "vr/containers/intrusive.h"
#include "vr/io/files.h"
#include "vr/io/sql/sqlite/utility.h"
#include "vr/io/uri.h"
#include "vr/settings.h"
#include "vr/mc/spinlock.h"
#include "vr/util/object_pools.h"

#include <mutex>

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
//............................................................................
//............................................................................
namespace
{

using connection                = sql_connection_factory::connection;

struct cfg_connection_cache; // forward

struct connection_pool_entry: public connection, // NOTE: keep this the 1st base
                              public intrusive::bi::slist_base_hook<intrusive::bi_traits::link_mode>
{
    cfg_connection_cache * m_parent { };

}; // end of class

using open_connection_list      = intrusive::bi::slist
                                <
                                    connection_pool_entry,
                                    intrusive::bi::constant_time_size<false>, // TODO 'true' if used for min/max pool management
                                    intrusive::bi::size_type<int32_t>
                                >;

using connection_pool_options   = util::pool_options<connection_pool_entry, int32_t, 4>::type;
using connection_pool           = util::object_pool<connection_pool_entry,  connection_pool_options>; // not expecting many connection instances

struct cfg_connection_cache final // protected by 'm_lock'
{
    using lock          = std::mutex;

    cfg_connection_cache (uri const & db_url, int32_t const db_open_flags, int32_t const pool_min_size) :
        m_db_url { db_url },
        m_db_open_flags { db_open_flags },
        m_pool_min_size { pool_min_size }
    {
        check_nonnegative (m_pool_min_size);
    }


    open_connection_list m_open_list { };
    uri const m_db_url;
    int32_t const m_db_open_flags;
    int32_t const m_pool_min_size;
    string_vector m_on_create_pragmas { };
    string_vector m_on_open_pragmas { };
    lock m_lock { };

}; // end of class

} // end of anonymous
//............................................................................
//............................................................................

#define vr_MEMORY_PATH  "mem"

template<>
struct sqlite_connection::access_by<sql_connection_factory> final
{
    static ::sqlite3 * & handle (sqlite_connection & c)
    {
        return c.m_handle;
    }

}; // end of class
//............................................................................

struct sql_connection_factory::pimpl final
{
    using connection_cache_map         = boost::unordered_map</* cfg name */std::string, std::unique_ptr<cfg_connection_cache>>;


    VR_ASSUME_COLD pimpl (settings const & named_cfgs)
    {
        check_condition (named_cfgs.is_object (), named_cfgs.type ());

        for (auto i = named_cfgs.begin (); i != named_cfgs.end (); ++ i)
        {
            std::string const & name = i.key ();
            settings const & cfg = i.value ();

            // 'db'         : <filename> or missing
            // 'mode'       : 'ro', 'rw', 'rwc', 'memory' (implies 'rwc')
            // 'cache'      : 'private', 'shared' (default)
            // 'pool_size'  : <nonnegative int> (default: 0)

            std::string const mode = cfg.at ("mode");
            std::string const cache = cfg.value ("cache", "shared");
            int32_t const pool_min_size = cfg.value ("pool_size", 0);

            std::string db_uri_str { };

            if (mode == "memory")
            {
                db_uri_str = "file:" vr_MEMORY_PATH "?mode=memory";
            }
            else
            {
                std::string const db_file_or_url = cfg.at ("db").get<std::string> ();
                try
                {
                    uri const db_url { db_file_or_url };

                    db_uri_str = db_url.native () + "?mode=" + mode;
                }
                catch (...)
                {
                    fs::path const db_file { io::weak_canonical_path (db_file_or_url) };

                    db_uri_str = "file://" + db_file.native () + "?mode=" + mode;
                }
            }

            db_uri_str += "&cache="; db_uri_str += cache;

            // TODO ASX-140: currently ro connections will lock out the db file for r/w

            uri const db_url { db_uri_str };
            int32_t const db_open_flags { SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_URI | SQLITE_OPEN_NOMUTEX }; // note: this must not be less permissive than URI 'mode'

            auto const rc = m_cache_map.emplace (name, std::make_unique<cfg_connection_cache> (db_url, db_open_flags, pool_min_size));
            if (! rc.second)
                throw_x (invalid_input, "duplicate sql connection cfg name " + print (name));

            if (cfg.find ("on_create") != cfg.end ())
            {
                json const & a = cfg ["on_create"];
                check_condition (a.is_array ());

                string_vector & pragmas = rc.first->second->m_on_create_pragmas;

                for (std::string const & s : a)
                {
                    pragmas.push_back (s);
                }

                pragmas.shrink_to_fit ();
            }

            if (cfg.find ("on_open") != cfg.end ())
            {
                json const & a = cfg ["on_open"];
                check_condition (a.is_array ());

                string_vector & pragmas = rc.first->second->m_on_open_pragmas;

                for (std::string const & s : a)
                {
                    pragmas.push_back (s);
                }

                pragmas.shrink_to_fit ();
            }
        }
    }

    VR_ASSUME_COLD ~pimpl ()
    {
        cleanup ();
    }


    VR_ASSUME_COLD void start ()
    {
        // populate connection caches with non-zero min sizes:

        for (auto & kv : m_cache_map)
        {
            cfg_connection_cache & cache = * kv.second;

            if (cache.m_pool_min_size > 0)
            {
                LOG_trace1 << "warming up connection pool " << print (kv.first) << " to " << cache.m_pool_min_size << " open connection(s)";

                for (int32_t i = 0; i < cache.m_pool_min_size; ++ i)
                {
                    auto const ar = m_connection_pool.allocate ();

                    connection_pool_entry & cpe = std::get<0> (ar);

                    ::sqlite3 * & handle { sqlite_connection::access_by<sql_connection_factory>::handle (cpe) };
                    open_connection (cache, handle);

                    LOG_trace2 << "  opened connection handle @" << handle;

                    cpe.m_parent = & cache;
                    cache.m_open_list.push_front (cpe);
                }
            }
        }

        // TODO release sqlite memory
    }

    VR_ASSUME_COLD void stop ()
    {
        cleanup ();
    }

    VR_ASSUME_COLD void cleanup () VR_NOEXCEPT // idempotent
    {
        try
        {
            if (intrusive::bi_traits::safe_mode ()) // compile-time guard
            {
                // in safe linking mode, explicit intrusive list clearing is necessary, otherwise
                // bucket/item destructors will complain about being destroyed in still-linked state:

                for (auto & kv : m_cache_map)
                {
                    cfg_connection_cache & cache = * kv.second;
                    cache.m_open_list.clear ();
                }
            }

            m_cache_map.clear ();
        }
        catch (std::exception const & e)
        {
            LOG_error << "cleanup failure: " << exc_info (e);
        }
    }



    connection & acquire (std::string const & name)
    {
        LOG_trace2 << "acquiring connection for pool " << print (name) << " ...";

        auto const i = m_cache_map.find (name);
        if (VR_UNLIKELY (i == m_cache_map.end ()))
            throw_x (invalid_input, "invalid sql configuration name " + print (name));

        cfg_connection_cache & cache = * i->second;
        LOG_trace2 << "connecting to db " << print (cache.m_db_url) << " ...";

        {
            std::lock_guard<cfg_connection_cache::lock> _1 { cache.m_lock }; // lock #1 (could be long-lasting in the slow path)

            if (VR_LIKELY (! cache.m_open_list.empty ())) // fast path
            {
                connection_pool_entry & cpe = cache.m_open_list.front ();
                assert_eq (cpe.m_parent, & cache);
                cache.m_open_list.pop_front ();

                return cpe;
            }

            // else [slow path] allocate and connect a new 'm_connection_pool' slot:

            LOG_trace2 << "  growing connection pool " << print (name) << " ...";

            connection_pool_entry * cpe { nullptr };
            {
                std::lock_guard<lock> _2 { m_lock }; // lock #2, acquired while still holding lock #1

                auto const ar = m_connection_pool.allocate ();
                cpe = & std::get<0> (ar);
            }
            assert_nonnull (cpe);

            {
                ::sqlite3 * & handle { sqlite_connection::access_by<sql_connection_factory>::handle (* cpe) };
                open_connection (cache, handle);
            }

            cpe->m_parent = & cache;
            return (* cpe);
        }
    }

    void release (connection const & c)
    {
        connection_pool_entry & cpe = static_cast<connection_pool_entry &> (const_cast<connection &> (c));
        assert_nonnull (cpe.m_parent);

        cfg_connection_cache & cache = * cpe.m_parent;

        {
            std::lock_guard<cfg_connection_cache::lock> _1 { cache.m_lock }; // lock #1

            cache.m_open_list.push_front (cpe);
        }
    }

    static void open_connection (cfg_connection_cache const & cfg, ::sqlite3 * & handle)
    {
        util::str_range const db_file = cfg.m_db_url.split ().path ();
        bool const creating = ((db_file == vr_MEMORY_PATH) || (! fs::exists (db_file.to_string ())));

        VR_CHECKED_SQLITE_CALL (::sqlite3_open_v2 (cfg.m_db_url.native ().c_str (), & handle, cfg.m_db_open_flags, nullptr));

        if (creating)
        {
            for (std::string const & pragma : cfg.m_on_create_pragmas)
            {
                LOG_trace1 << "    executing [" << pragma << "] ...";
                VR_CHECKED_SQLITE_CALL (::sqlite3_exec (handle, pragma.c_str (), nullptr, nullptr, nullptr));
            }
        }

        for (std::string const & pragma : cfg.m_on_open_pragmas)
        {
            LOG_trace1 << "    executing [" << pragma << "] ...";
            VR_CHECKED_SQLITE_CALL (::sqlite3_exec (handle, pragma.c_str (), nullptr, nullptr, nullptr));
        }

        // TODO release sqlite memory
    }


    using lock          = mc::spinlock<>;


    connection_cache_map m_cache_map { }; // immutable mapping after construction
    connection_pool m_connection_pool { 2 }; // protected by 'm_lock'
    lock m_lock { };

}; // end of class
//............................................................................
//............................................................................

sql_connection_factory::sql_connection_factory (settings const & named_cfgs) :
    m_impl { std::make_unique<pimpl> (named_cfgs) }
{
}

sql_connection_factory::~sql_connection_factory ()    = default; // pimpl
//............................................................................

void
sql_connection_factory::start ()
{
    m_impl->start ();
}

void
sql_connection_factory::stop ()
{
    m_impl->stop ();
}
//............................................................................

connection &
sql_connection_factory::acquire (std::string const & name)
{
    return m_impl->acquire (name);
}

void
sql_connection_factory::release (connection const & c)
{
    m_impl->release (c);
}

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
