
#include "vr/io/sql/sqlite/sqlite_connection.h"

#include "vr/asserts.h"
#include "vr/io/sql/sqlite/utility.h"
#include "vr/util/logging.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{

sqlite_connection::~sqlite_connection () VR_NOEXCEPT
{
    ::sqlite3 * const handle = m_handle;
    m_handle = nullptr;

    if (handle)
    {
        LOG_trace2 << "closing sqlite connection handle @" << handle;
        VR_CHECKED_SQLITE_CALL_noexcept (::sqlite3_close_v2 (handle)); // null handle is also ok, per API docs
    }
}
//............................................................................

void
sqlite_connection::tx_begin ()
{
    DLOG_trace2 << "tx_begin: nest count " << m_nest_count;

    if (! (m_nest_count ++))
    {
        LOG_trace1 << "BEGIN " << m_handle;

        assert_nonnull (m_handle);
        VR_CHECKED_SQLITE_CALL (::sqlite3_exec (m_handle, "BEGIN;", nullptr, nullptr, nullptr));
    }
}

void
sqlite_connection::tx_commit ()
{
    DLOG_trace2 << "tx_commit: nest count " << m_nest_count;

    if (! (-- m_nest_count))
    {
        LOG_trace1 << "COMMIT " << m_handle;

        assert_nonnull (m_handle);
        VR_CHECKED_SQLITE_CALL (::sqlite3_exec (m_handle, "COMMIT;", nullptr, nullptr, nullptr));
    }
}

void
sqlite_connection::tx_rollback ()
{
    LOG_trace1 << "ROLLBACK " << m_handle;

    assert_nonnull (m_handle);
    VR_CHECKED_SQLITE_CALL (::sqlite3_exec (m_handle, "ROLLBACK;", nullptr, nullptr, nullptr));
    m_nest_count = { };
}

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
