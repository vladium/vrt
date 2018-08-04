
#include "vr/io/dao/object_DAO.h"

#include "vr/io/sql/sql_connection_factory.h"
#include "vr/io/sql/sqlite/sqlite_stmt_cache.h"
#include "vr/io/sql/sqlite/utility.h"
#include "vr/settings.h"
#include "vr/util/logging.h"
#include "vr/mc/spinlock.h"
#include "vr/utility.h"

#include <boost/algorithm/string/replace.hpp>
#include <boost/thread/tss.hpp>

#include <mutex>

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
//............................................................................

#define vr_STMT_FIND_AS_OF          "#fao.DAO"
#define vr_STMT_PERSIST_AS_OF_1     "#1.pao.DAO"
#define vr_STMT_PERSIST_AS_OF_2     "#2.pao.DAO"
#define vr_STMT_DELETE_AS_OF        "#dao.DAO"

//............................................................................

using connection            = impl::object_DAO_ifc::connection;

template<>
struct connection::access_by<object_DAO> final
{
    static inline ::sqlite3 * handle (connection const & c)
    {
        return c.m_handle;
    }

}; // end of class

inline ::sqlite3 *
c_handle (connection const & c) // just a shortcut
{
    return connection::access_by<object_DAO>::handle (c);
}
//............................................................................
//............................................................................
namespace
{
//............................................................................

using spin_lock         = mc::spinlock<>;
using mutex_lock        = std::mutex;

//............................................................................

void
null_delete (connection *)
{
    // TODO could try releasing back to the factory pool ?
}
//............................................................................

void
remove_top_namespace (std::string & cls_name)
{
    auto const i = cls_name.find ("::");
    if (VR_LIKELY (i != std::string::npos))
    {
        cls_name.erase (0, i + 2);
    }
}

void
to_table_name (std::string & cls_name)
{
    boost::replace_all (cls_name, "::", "_");
}
//............................................................................

static int32_t
table_info_callback (addr_t const data, int32_t col_count, char ** field_names, char ** col_names)
{
    (* static_cast<bool *> (data)) = true;
    return 0;
}

} // end of anonymous
//............................................................................
//............................................................................

struct object_DAO::ifc_context final
{
    ifc_context (object_DAO::pimpl & pimpl, bool const rw, std::string && table_name, std::string && key_name, impl::type_erased_fns && fns) :
        m_pimpl { pimpl },
        m_ifc { this, rw, table_name, key_name, fns.m_key_data_type },
        m_fns { std::move (fns) }, // note: last use of 'fns'
        m_table_name { std::move (table_name) }, // note: last use of 'table_name'
        m_key_name  { std::move (key_name) } // note: last use of 'key_name'
    {
    }


    VR_FORCEINLINE sql_connection_factory & sql_factory ();


    object_DAO::pimpl & m_pimpl;
    impl::object_DAO_ifc m_ifc; // construct this before moving into the fields below
    impl::type_erased_fns const m_fns;
    std::string const m_table_name;
    std::string const m_key_name;
    std::string const m_stmt_ID_find_as_of { m_table_name + vr_STMT_FIND_AS_OF };
    std::string const m_stmt_ID_persist_as_of_1 { m_table_name + vr_STMT_PERSIST_AS_OF_1 };
    std::string const m_stmt_ID_persist_as_of_2 { m_table_name + vr_STMT_PERSIST_AS_OF_2 };
    std::string const m_stmt_ID_delete_as_of { m_table_name + vr_STMT_DELETE_AS_OF };
    boost::thread_specific_ptr<connection> m_ro_c_tls_cached { & null_delete }; // note: not 'static' by design
    boost::thread_specific_ptr<connection> m_rw_c_tls_cached { & null_delete }; // note: not 'static' by design

}; // end of class
//............................................................................
//............................................................................

struct object_DAO::pimpl final
{
    pimpl (object_DAO & parent, settings const & cfg) :
        m_parent { parent },
        m_ro_cfg_name { cfg.at ("cfg.ro").get<std::string> () },
        m_rw_cfg_name { cfg.value ("cfg.rw", "") }
    {
        LOG_trace1 << "configured with cfgs {ro: " << print (m_ro_cfg_name) << ", rw: " << print (m_rw_cfg_name) << '}';
    }


    VR_FORCEINLINE bool read_only () const
    {
        return m_rw_cfg_name.empty ();
    }

    VR_ASSUME_HOT addr_t ifc (std::type_index const & tix, std::string && key_name, impl::type_erased_fns && fns, bool const rw)
    {
        {
            std::lock_guard<spin_lock> _1 { m_type_map_ll_lock }; // acquire a light-weight lock first

            auto i = m_type_map.find (tix);
            if (VR_LIKELY (i != m_type_map.end ()))
            {
                ifc_context * const ifc { i->second.get () };
                assert_nonnull (ifc);
                assert_eq (key_name, ifc->m_key_name); // guard for the same persistent type being used consistently with the same key

                return (& ifc->m_ifc); // note: this is a type unsafe HACK, but is safe as long as 'r{o,w}_ifc_impl<T...>' are "standard layout"
            }

            std::string cls_name { util::class_name (tix) };

            remove_top_namespace (cls_name); // TODO useful/needed ?
            to_table_name (cls_name);

            {
                std::lock_guard<mutex_lock> _2 { m_type_map_hl_lock }; // acquire a costlier lock next

                i = m_type_map.emplace (tix, std::make_unique<ifc_context> (* this, rw, std::move (cls_name), std::move (key_name), std::move (fns))).first;

                return (& i->second->m_ifc);
            }
        }
    }

    /*
     * look up or create
     *
     * note: caller does locking
     */
    sqlite_stmt_cache & stmt_cache_for (::sqlite3 * const handle)
    {
        assert_nonnull (handle);

        auto i = m_stmt_cache_map.find (handle);
        if (VR_UNLIKELY (i == m_stmt_cache_map.end ()))
        {
            i = m_stmt_cache_map.emplace (handle, std::make_unique<sqlite_stmt_cache> ()).first;
        }
        return (* i->second);
    }


    using type_ifc_map      = boost::unordered_map<std::type_index, std::unique_ptr<ifc_context>>; // note: value addrs are stable
    using stmt_cache        = boost::unordered_map<::sqlite3 *, std::unique_ptr<sqlite_stmt_cache>>;


    object_DAO & m_parent;
    std::string const m_ro_cfg_name;
    std::string const m_rw_cfg_name;
    type_ifc_map m_type_map { };        // protected by 'm_type_*_lock's
    stmt_cache m_stmt_cache_map { };    // protected by 'm_type_*_lock's
    mutex_lock m_type_map_hl_lock { };
    spin_lock m_type_map_ll_lock { };   // cl-padded, keep as the last field

}; // end of class
//............................................................................
/*
 * has to be defined after the 'object_DAO::pimpl' definition has been seen
 */
inline sql_connection_factory &
object_DAO::ifc_context::sql_factory ()
{
    assert_nonnull (m_pimpl.m_parent.m_sql_factory);

    return (* m_pimpl.m_parent.m_sql_factory);
}
//............................................................................
//............................................................................

object_DAO::object_DAO (settings const & cfg) :
    m_impl { std::make_unique<pimpl> (* this, cfg) }
{
    dep (m_sql_factory) = "sql";
}

object_DAO::~object_DAO ()    = default; // pimpl
//............................................................................

bool
object_DAO::read_only () const
{
    return m_impl->read_only ();
}
//............................................................................

addr_t
object_DAO::ro_ifc (std::type_index const & tix, std::string && key_name, impl::type_erased_fns && fns) const
{
    return m_impl->ifc (tix, std::move (key_name), std::move (fns), /* read/write */false);
}

addr_t
object_DAO::rw_ifc (std::type_index const & tix, std::string && key_name, impl::type_erased_fns && fns)
{
    return m_impl->ifc (tix, std::move (key_name), std::move (fns), /* read/write */true);
}
//............................................................................
//............................................................................
namespace impl
{
//............................................................................

void
bind (::sqlite3_stmt * const stmt, string_literal_t const name, int32_t const value)
{
    VR_CHECKED_SQLITE_CALL (::sqlite3_bind_int (stmt, ::sqlite3_bind_parameter_index (stmt, name), value));
}

void
bind (::sqlite3_stmt * const stmt, string_literal_t const name, std::string const & value)
{
    VR_CHECKED_SQLITE_CALL (::sqlite3_bind_text (stmt, ::sqlite3_bind_parameter_index (stmt, name), value.data (), value.size (), SQLITE_TRANSIENT));
}
//............................................................................

object_DAO_ifc::object_DAO_ifc (addr_t/* ifc_context */const ifc_ctx, bool const rw, std::string const & table_name, std::string const & key_name, string_literal_t const key_data_type) :
    m_context { ifc_ctx }
{
    assert_nonnull (m_context);

    // else create a new table for this type:

    LOG_trace1 << "using table [" << table_name << "], ID column [" << key_name << ']';

    object_DAO::ifc_context & ctx = context<object_DAO::ifc_context> (); // note: can't access anything other than sql_factory() in this constructor yet

    auto & c = ctx.sql_factory ().acquire (rw ? ctx.m_pimpl.m_rw_cfg_name : ctx.m_pimpl.m_ro_cfg_name);
    VR_SCOPE_EXIT ([& ctx, & c, this]() { ctx.sql_factory ().release (c); });

    if (rw)
    {
        std::string sql { };
        {
            std::stringstream ss { };
            ss << "CREATE TABLE IF NOT EXISTS [" << table_name << "] "
                  "("
                  " [vb] INTEGER NOT NULL,"
                  " [ve] INTEGER,"
                  " [" << key_name << "] " << key_data_type << " NOT NULL,"
               << " [obj] JSON NOT NULL, "
                  " CONSTRAINT pk_vb_key PRIMARY KEY ([vb], [" << key_name << "]) "
                  ")"
                  ";"; // TODO WITHOUT ROWID ?
            sql = ss.str ();
        }

        LOG_trace2 << '[' << sql << ']';

        VR_CHECKED_SQLITE_CALL (::sqlite3_exec (c_handle (c), sql.c_str (), nullptr, nullptr, nullptr));
    }
    else // check table 'table_name' exists
    {
        std::string const sql { "PRAGMA table_info ([" + table_name + "])" };

        LOG_trace2 << '[' << sql << ']';

        bool table_exists { false };
        VR_CHECKED_SQLITE_CALL (::sqlite3_exec (c_handle (c), sql.c_str (), & table_info_callback, & table_exists, nullptr));

        if (VR_UNLIKELY (! table_exists))
            throw_x (illegal_state, "DAO created in read-only mode, but table [" + table_name + "] does not exist");
    }
}
//............................................................................

tx_guard<connection>
object_DAO_ifc::tx_begin_ro ()
{
    object_DAO::ifc_context & ctx = context<object_DAO::ifc_context> ();

    connection * c = ctx.m_ro_c_tls_cached.get ();
    if (VR_UNLIKELY (c == nullptr))
    {
        c = & ctx.sql_factory ().acquire (ctx.m_pimpl.m_ro_cfg_name);
        ctx.m_ro_c_tls_cached.reset (c);

        DLOG_trace2 << "using TLS-cached ro connection " << c;
    }

    assert_nonnull (c);
    return { * c }; // this actually invokes 'c->tx_begin ()'
}

tx_guard<connection>
object_DAO_ifc::tx_begin_rw ()
{
    object_DAO::ifc_context & ctx = context<object_DAO::ifc_context> ();

    connection * c = ctx.m_rw_c_tls_cached.get ();
    if (VR_UNLIKELY (c == nullptr))
    {
        c = & ctx.sql_factory ().acquire (ctx.m_pimpl.m_rw_cfg_name);
        ctx.m_rw_c_tls_cached.reset (c);

        DLOG_trace2 << "using TLS-cached rw connection " << c;
    }

    assert_nonnull (c);
    return { * c }; // this actually invokes 'c->tx_begin ()'
}
//............................................................................

void
object_DAO_ifc::find_as_of (std::type_index const & tix, call_traits<util::date_t>::param date, addr_const_t/* ID_value_type */ const ID, addr_t/* optional<T> */ const optional_obj)
{
    assert_condition (! date.is_special (), date);
    assert_nonnull (ID);
    assert_nonnull (optional_obj);

    object_DAO::ifc_context & ctx = context<object_DAO::ifc_context> ();

    int32_t const sb = util::date_as_int (date);

    DLOG_trace1 << "querying v[" << sb << "] obj ...";

    connection * c { };
    bool c_scoped { true }; // default to 'c' being scoped to this method
    {
        c = ctx.m_ro_c_tls_cached.get (); // note: check a value possibly acquired by 'tx_begin()'
        if (c)
            c_scoped = false;
        else
            c = & ctx.sql_factory ().acquire (ctx.m_pimpl.m_ro_cfg_name);
    }

    VR_SCOPE_EXIT ([& ctx, c, c_scoped, this]() { if (c_scoped) ctx.sql_factory ().release (* c); });


    // acquire stmt cache associated with 'c's handle, if any:

    ::sqlite3 * const handle = c_handle (* c);

    ::sqlite3_stmt * stmt { }; // assigned below
    {
        std::string const & stmt_ID = ctx.m_stmt_ID_find_as_of;

        std::lock_guard<spin_lock> _1 { ctx.m_pimpl.m_type_map_ll_lock }; // acquire a light-weight lock first

        sqlite_stmt_cache & stmt_cache = ctx.m_pimpl.stmt_cache_for (handle);

        stmt = stmt_cache.cached (stmt_ID);
        if (VR_UNLIKELY (stmt == nullptr))
        {
            std::string sql { };
            {
                std::stringstream ss { };

                ss << "SELECT [obj] "
                      "FROM [" << ctx.m_table_name << "] "
                      "WHERE [" << ctx.m_key_name << "] = @id AND ([vb] <= @vb AND (@vb < [ve] OR [ve] IS NULL)) "
                      ";";

                sql = ss.str ();
            }

            DLOG_trace2 << "SQL: [" << sql << ']';

            {
                std::lock_guard<mutex_lock> _2 { ctx.m_pimpl.m_type_map_hl_lock }; // acquire a costlier lock next

                stmt = stmt_cache.prepare (stmt_ID, sql, * c);
            }
        }
    }
    assert_nonnull (stmt);

    // bind '@vb', '@id':

    VR_CHECKED_SQLITE_CALL (::sqlite3_bind_int (stmt, ::sqlite3_bind_parameter_index (stmt, "@vb"), sb));
    ctx.m_fns.m_bifn (stmt, ID, "@id");

    // execute:

    VR_IF_DEBUG (int32_t row_count { };)

    while (true)
    {
        int32_t const rc = ::sqlite3_step (stmt);

        switch (rc) // TODO handle BUSY
        {
            case SQLITE_ROW:
            {
                string_literal_t const c_str = reinterpret_cast<string_literal_t> (::sqlite3_column_text (stmt, 0));
                if (c_str == nullptr) // NULL 'obj'
                    return;

                VR_IF_DEBUG // ensure the query returns 1 row at most
                (
                    DLOG_trace1 << "got 'obj' row: " << c_str;
                    ++ row_count;
                    assert_le (row_count, 1, c_str);
                )

                ctx.m_fns.m_ufn ({ c_str, static_cast<int32_t> (std::strlen (c_str)) }, optional_obj);
            };
            break;

            case SQLITE_DONE:
                return;

            default:
                throw_x (io_exception, "sqlite statement step error (" + string_cast (rc) + "): " + ::sqlite3_errstr (rc));

        } // end of switch
    }
}
//............................................................................

bool
object_DAO_ifc::persist_as_of (std::type_index const & tix, call_traits<util::date_t>::param date, addr_const_t/* T */ const obj)
{
    assert_condition (! date.is_special (), date);
    assert_nonnull (obj);

    object_DAO::ifc_context & ctx = context<object_DAO::ifc_context> ();

    std::string obj_str { };
    {
        std::stringstream obj_os { };
        ctx.m_fns.m_mfn (obj, obj_os);

        obj_str = obj_os.str ();
    }

    int32_t const sb = util::date_as_int (date);

    DLOG_trace1 << "persisting v[" << sb << "] obj ...";
    DLOG_trace2 << "  " << obj_str;

    connection * c { };
    bool c_scoped { true }; // default to 'c' being scoped to this method
    {
        c = ctx.m_rw_c_tls_cached.get (); // note: check a value possibly acquired by 'tx_begin()'
        if (c)
            c_scoped = false;
        else
            c = & ctx.sql_factory ().acquire (ctx.m_pimpl.m_rw_cfg_name);
    }

    VR_SCOPE_EXIT ([& ctx, c, c_scoped, this]() { if (c_scoped) ctx.sql_factory ().release (* c); });

    // acquire stmt cache associated with 'c's handle, if any:

    ::sqlite3 * const handle = c_handle (* c);

    VR_IF_DEBUG
    (
        if (::sqlite3_get_autocommit (handle)) LOG_warn << "connection " << handle << " is in autocommit mode";
    )

    ::sqlite3_stmt * stmt_1 { }; // assigned below
    ::sqlite3_stmt * stmt_2 { }; // assigned below
    {
        std::string const & stmt_1_ID = ctx.m_stmt_ID_persist_as_of_1;
        std::string const & stmt_2_ID = ctx.m_stmt_ID_persist_as_of_2;

        std::lock_guard<spin_lock> _1 { ctx.m_pimpl.m_type_map_ll_lock }; // acquire a light-weight lock first

        sqlite_stmt_cache & stmt_cache = ctx.m_pimpl.stmt_cache_for (handle);

        stmt_1 = stmt_cache.cached (stmt_1_ID);
        if (VR_LIKELY (stmt_1 != nullptr))
            stmt_2 = stmt_cache.cached (stmt_2_ID);
        else
        {
            std::string sql_1 { };
            std::string sql_2 { };

            {
                std::stringstream ss { };

                // TODO use a dummy "singleton" table to remove some expression repeats?
                // TODO UNION instead of a join?

                // TODO add "AND old.[ve] < @vb" to the join? need to log query plans

                ss << "WITH new ([vb], [ve], [" << ctx.m_key_name << "], [obj]) AS (VALUES (@vb, NULL, @id, @obj)) "
                      "INSERT INTO [" << ctx.m_table_name << "] "
                      "SELECT new.* "
                      "FROM (new LEFT JOIN [" << ctx.m_table_name << "] AS old ON old.[" << ctx.m_key_name << "] = @id AND old.[ve] IS NULL) "
                      "WHERE new.[obj] <> old.[obj] OR old.[obj] IS NULL "
                      ";";

                sql_1 = ss.str ();
            }
            {
                std::stringstream ss { };

                // TODO easier to detect an insert at the level of stmt_step() return?

                ss << "UPDATE [" << ctx.m_table_name << "] "
                      "SET [ve] = @vb "
                      "WHERE [" << ctx.m_key_name << "] = @id AND [vb] < @vb AND [ve] IS NULL "
                      ";";

                sql_2 = ss.str ();
            }

            DLOG_trace2 << "SQL, step 1: [" << sql_1 << ']';
            DLOG_trace2 << "SQL, step 2: [" << sql_2 << ']';

            {
                std::lock_guard<mutex_lock> _2 { ctx.m_pimpl.m_type_map_hl_lock }; // acquire a costlier lock next

                stmt_1 = stmt_cache.prepare (stmt_1_ID, sql_1, * c);
                stmt_2 = stmt_cache.prepare (stmt_2_ID, sql_2, * c);
            }
        }
    }
    assert_nonnull (stmt_1);
    assert_nonnull (stmt_2);

    // bind '@vb', '@id', '@obj':

    VR_CHECKED_SQLITE_CALL (::sqlite3_bind_int (stmt_1, ::sqlite3_bind_parameter_index (stmt_1, "@vb"), sb));
    ctx.m_fns.m_bofn (stmt_1, obj, "@id");
    VR_CHECKED_SQLITE_CALL (::sqlite3_bind_text (stmt_1, ::sqlite3_bind_parameter_index (stmt_1, "@obj"), obj_str.data (), obj_str.size (), SQLITE_TRANSIENT));

    // execute step 1:

    while (true)
    {
        int32_t const rc = ::sqlite3_step (stmt_1);

        if (VR_LIKELY (rc == SQLITE_DONE))
            break;
        else if (VR_UNLIKELY (rc != SQLITE_ROW)) // TODO handle BUSY
            throw_x (io_exception, "sqlite statement step error (" + string_cast (rc) + "): " + ::sqlite3_errstr (rc));
    }

    int32_t const rows_changed = ::sqlite3_changes (handle); // note: no races with other threads 'cause we still have the connection

    if (rows_changed)
    {
        // bind '@vb', '@id':

        VR_CHECKED_SQLITE_CALL (::sqlite3_bind_int (stmt_2, ::sqlite3_bind_parameter_index (stmt_2, "@vb"), sb));
        ctx.m_fns.m_bofn (stmt_2, obj, "@id");

        // execute step 2:

        while (true)
        {
            int32_t const rc = ::sqlite3_step (stmt_2);

            if (VR_LIKELY (rc == SQLITE_DONE))
                break;
            else if (VR_UNLIKELY (rc != SQLITE_ROW)) // TODO handle BUSY
                throw_x (io_exception, "sqlite statement step error (" + string_cast (rc) + "): " + ::sqlite3_errstr (rc));
        }
    }

    return rows_changed;
}
//............................................................................

bool
object_DAO_ifc::delete_as_of (std::type_index const & tix, call_traits<util::date_t>::param date, addr_const_t/* ID_value_type */ const ID)
{
    assert_condition (! date.is_special (), date);
    assert_nonnull (ID);

    object_DAO::ifc_context & ctx = context<object_DAO::ifc_context> ();

    int32_t const se = util::date_as_int (date);

    connection * c { };
    bool c_scoped { true }; // default to 'c' being scoped to this method
    {
        c = ctx.m_rw_c_tls_cached.get (); // note: check a value possibly acquired by 'tx_begin()'
        if (c)
            c_scoped = false;
        else
            c = & ctx.sql_factory ().acquire (ctx.m_pimpl.m_rw_cfg_name);
    }

    VR_SCOPE_EXIT ([& ctx, c, c_scoped, this]() { if (c_scoped) ctx.sql_factory ().release (* c); });

    // acquire stmt cache associated with 'c's handle, if any:

    ::sqlite3 * const handle = c_handle (* c);

    VR_IF_DEBUG
    (
        if (::sqlite3_get_autocommit (handle)) LOG_warn << "connection " << handle << " is in autocommit mode";
    )

    ::sqlite3_stmt * stmt { }; // assigned below
    {
        std::string const & stmt_ID = ctx.m_stmt_ID_delete_as_of;

        std::lock_guard<spin_lock> _1 { ctx.m_pimpl.m_type_map_ll_lock }; // acquire a light-weight lock first

        sqlite_stmt_cache & stmt_cache = ctx.m_pimpl.stmt_cache_for (handle);

        stmt = stmt_cache.cached (stmt_ID);
        if (VR_UNLIKELY (stmt == nullptr))
        {
            std::string sql { };
            {
                std::stringstream ss { };

                ss << "UPDATE [" << ctx.m_table_name << "] "
                      "SET [ve] = @ve "
                      "WHERE [" << ctx.m_key_name << "] = @id AND [ve] IS NULL "
                      ";";

                sql = ss.str ();
            }

            DLOG_trace2 << "SQL: [" << sql << ']';

            {
                std::lock_guard<mutex_lock> _2 { ctx.m_pimpl.m_type_map_hl_lock }; // acquire a costlier lock next

                stmt = stmt_cache.prepare (stmt_ID, sql, * c);
            }
        }
    }
    assert_nonnull (stmt);

    // bind '@ve', '@id':

    VR_CHECKED_SQLITE_CALL (::sqlite3_bind_int (stmt, ::sqlite3_bind_parameter_index (stmt, "@ve"), se));
    ctx.m_fns.m_bifn (stmt, ID, "@id");

    // execute:

    while (true)
    {
        int32_t const rc = ::sqlite3_step (stmt);

        if (VR_LIKELY (rc == SQLITE_DONE))
            break;
        else if (VR_UNLIKELY (rc != SQLITE_ROW)) // TODO handle BUSY
            throw_x (io_exception, "sqlite statement step error (" + string_cast (rc) + "): " + ::sqlite3_errstr (rc));
    }

    int32_t const rows_changed = ::sqlite3_changes (handle); // note: no races with other threads 'cause we still have the connection

    return rows_changed;
}

} // end of 'impl'
//............................................................................
//............................................................................
} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
