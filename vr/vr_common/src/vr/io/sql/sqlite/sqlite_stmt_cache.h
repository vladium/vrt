#pragma once

#include "vr/io/sql/sqlite/sqlite_connection.h"

struct sqlite3_stmt; // forward

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
/**
 * NOTE up to the caller to make sure this is used with the same connection
 *      instance and appropriate locking
 */
class sqlite_stmt_cache final: noncopyable
{
    public: // ...............................................................

        sqlite_stmt_cache ()    = default;
        ~sqlite_stmt_cache () VR_NOEXCEPT;

        // ACCESSORs:

        VR_ASSUME_HOT ::sqlite3_stmt * cached (std::string const ID) const;

        // MUTATORs:

        ::sqlite3_stmt * prepare (std::string const ID, std::string const & sql, sqlite_connection const & c);

    private: // ..............................................................

        using stmt_map      = boost::unordered_map<std::string, ::sqlite3_stmt *>;

        stmt_map m_stmt_map { };
        VR_IF_DEBUG (::sqlite3 * m_handle { };); // correct usage guard in debug builds

}; // end of class

} // end of 'op'
} // end of namespace
//----------------------------------------------------------------------------
