#pragma once

#include "vr/macros.h"
#include "vr/types.h"

struct sqlite3; // forward

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
class sqlite_stmt_cache; // forward

/*
 * NOTEs when using shared cache:
 *
 * (1) "At most one connection to a single shared cache may open a write transaction at any one time.
 *      This may co-exist with any number of read transactions."
 *
 * (2) "When two or more connections use a shared-cache, locks are used to serialize concurrent access
 *      attempts on a per-table basis. Tables support two types of locks, "read-locks" and "write-locks".
 *      Locks are granted to connections - at any one time, each database connection has either a read-lock,
 *      write-lock or no lock on each database table.
 *
 *      At any one time, a single table may have any number of active read-locks or a single active write lock.
 *      To read data a table, a connection must first obtain a read-lock. To write to a table, a connection
 *      must obtain a write-lock on that table. If a required table lock cannot be obtained, the query fails
 *      and SQLITE_LOCKED is returned to the caller."
 *
 * (3) "A database connection in read-uncommitted mode does not attempt to obtain read-locks before reading
 *      from database tables as described above. This can lead to inconsistent query results if another database
 *      connection modifies a table while it is being read, but it also means that a read-transaction opened
 *      by a connection in read-uncommitted mode can neither block nor be blocked by any other connection."
 *
 * (4) restrictions on pinning connection to the thread that called sqlite3_open() in shared_cache mode
 *     were dropped with sqlite v3.5.0; restrictions on using shared-cache mode with virtual tables were
 *     removed in sqlite v3.6.17;
 *
 * (5) shared cache can be used with in-memory dbs PROVIDED they were created
 *     using a URI filename;
 */
/**
 * TODO make this a scoped RAII class?
 *
 */
class sqlite_connection: noncopyable
{
    public: // ...............................................................

        ~sqlite_connection () VR_NOEXCEPT;


        // MUTATORs:
        // note: these are not atomic or locked in any way:

        void tx_begin ();
        void tx_commit ();
        void tx_rollback ();


        template<typename A> class access_by;

    protected: // ............................................................

        sqlite_connection ()    = default;

    private: // ..............................................................

        friend class sqlite_stmt_cache;
        template<typename A> friend class access_by;

        ::sqlite3 * m_handle { }; // [owning]
        int32_t m_nest_count { }; // note:

}; // end of class

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
