
#include "vr/io/sql/sqlite/sqlite_stmt_cache.h"

#include "vr/asserts.h"
#include "vr/io/sql/sqlite/utility.h"
#include "vr/util/logging.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
//............................................................................

sqlite_stmt_cache::~sqlite_stmt_cache () VR_NOEXCEPT
{
    LOG_trace1 << "finalizing " << m_stmt_map.size () << " prepared statement(s)";

    for (auto const & kv : m_stmt_map)
    {
        VR_CHECKED_SQLITE_CALL_noexcept (::sqlite3_finalize (kv.second));
    }
}
//............................................................................

::sqlite3_stmt *
sqlite_stmt_cache::cached (std::string const ID) const
{
    auto const i = m_stmt_map.find (ID);
    if (VR_LIKELY (i != m_stmt_map.end ()))
    {
        ::sqlite3_stmt * const stmt = i->second;
        DLOG_trace2 << "resetting " << print (ID) << " ...";

        VR_CHECKED_SQLITE_CALL (::sqlite3_reset (stmt));
        return stmt;
    }

    return nullptr;
}
//............................................................................

::sqlite3_stmt *
sqlite_stmt_cache::prepare (std::string const ID, std::string const & sql, sqlite_connection const & c)
{
    VR_IF_DEBUG (if (m_handle) check_eq (m_handle, c.m_handle, ID, sql); )

    ::sqlite3 * const handle = c.m_handle;
    assert_nonnull (handle);

    ::sqlite3_stmt * stmt { };

    LOG_trace1 << "preparing " << print (ID) << ": [" << sql << "] ...";

    /*
     * "If the caller knows that the supplied string is nul-terminated, then there is a small performance advantage
     *  to passing an nByte parameter that is the number of bytes in the input string *including* the nul-terminator."
     */
    VR_CHECKED_SQLITE_CALL (::sqlite3_prepare_v2 (handle, sql.c_str (), sql.size () +/* ! */1, & stmt, nullptr));
    assert_nonnull (stmt);

    if (VR_UNLIKELY (! m_stmt_map.emplace (ID, stmt).second)) // an ID clash or incorrect usage
    {
        VR_CHECKED_SQLITE_CALL_noexcept (::sqlite3_finalize (stmt));
        throw_x (invalid_input, "statement cache for db handle @" + string_cast (handle) + " already has a " + print (ID) + " entry");
    }

    VR_IF_DEBUG (if (m_handle == nullptr) m_handle = handle; )
    return stmt;
}

} // end of 'op'
} // end of namespace
//----------------------------------------------------------------------------
