
#include "vr/io/tar/iterator.h"

#include "vr/exceptions.h"
#include "vr/io/files.h"
#include "vr/sys/os.h"
#include "vr/util/logging.h"
#include "vr/util/memory.h" // make_unique
#include "vr/utility.h"

#include <archive.h>
#include <archive_entry.h>

#if !defined (_LARGEFILE64_SOURCE)
#   define _LARGEFILE64_SOURCE // lseek64()
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define vr_trace_io     2

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
//............................................................................
//............................................................................
namespace tar
{

iterator::iterator_impl::iterator_impl (iterator & parent) :
    m_handle { ::archive_read_new (), ::archive_read_free },
    m_current { parent }
{
}

iterator::iterator_impl::~iterator_impl ()
{
    close ();
}

void
iterator::iterator_impl::close () VR_NOEXCEPT
{
    if (m_handle) LOG_trace1 << "closing archive ...";

    m_handle.reset ();
    m_iter_buf.reset ();

    int32_t h = m_fd;
    if (h >= 0)
    {
        m_fd = -1;
        VR_CHECKED_SYS_CALL_noexcept (::close (h));
    }
}
//............................................................................

VR_FORCEINLINE void
iterator::iterator_impl::set_entry_buf (int64_t const buf_capacity)
{
    m_buf_capacity = buf_capacity;
}

VR_FORCEINLINE void
iterator::iterator_impl::reset_entry_buf ()
{
    m_buf_capacity = io_iter_buf_size;
}
//............................................................................
//............................................................................

iterator::iterator (fs::path const & file) :
    m_impl { std::make_shared<iterator_impl> (* this) }
{
    assert_condition (m_impl->m_handle); // default constructor sets this up

    LOG_trace1 << "opening archive [" << io::absolute_path (file).native () << "] ...";
    m_impl->m_fd = VR_CHECKED_SYS_CALL (::open (file.c_str (), O_RDONLY));

    m_impl->m_iter_buf = boost::make_unique_noinit<int8_t []> (io_data_buf_size); // allocate mem only after file opened ok
    assert_condition (m_impl->m_buf_capacity == io_iter_buf_size);

    ::archive * const a = m_impl->m_handle.get ();

    ::archive_read_support_filter_all (a);
    ::archive_read_support_format_all (a);

    ::archive_read_set_callback_data (a, m_impl.get ());
    ::archive_read_set_read_callback (a, read_callback);
    ::archive_read_set_skip_callback (a, skip_callback);

    if (VR_UNLIKELY (::archive_read_open1 (a) != ARCHIVE_OK))
        throw_x (io_exception, "failed to open archive [" + io::absolute_path (file).native () + "]: " + ::archive_error_string (a));

    increment ();
}
//............................................................................

void
iterator::increment ()
{
    iterator_impl & ii = * m_impl;

    ::archive * const a = ii.m_handle.get ();
    ::archive_entry * entry { nullptr };

    switch (VR_LIKELY_VALUE (::archive_read_next_header (a, & entry), ARCHIVE_OK))
    {
        case ARCHIVE_OK:
        {
            ii.m_current.m_name = ::archive_entry_pathname (entry);

            check_condition (::archive_entry_size_is_set (entry), ii.m_current.m_name);
            ii.m_current.m_size = ::archive_entry_size (entry);
        }
        return;

        case ARCHIVE_EOF:
        {
            ii.close ();
        }
        return;

        default:
        {
            throw_x (io_exception, ::archive_error_string (a));
        }
        /* return; */

    } // end of switch
}
//............................................................................

VR_ASSUME_HOT signed_size_t
iterator::read_callback (::archive * const a, void/*  iterator_impl */ * const ctx, const void ** const buffer)
{
    assert_nonnull (ctx);
    const iterator_impl & ii = * static_cast<const iterator_impl *> (ctx);

    LOG_trace (vr_trace_io) << "read_callback (" << a << ", " << ctx << ", " << buffer << "), buf capacity " << ii.m_buf_capacity;

    assert_nonnegative (ii.m_fd);

    int8_t * const buf = ii.m_iter_buf.get ();
    assert_condition (buf);

    (* buffer) = buf;

    signed_size_t const rc = VR_CHECKED_SYS_CALL (::read (ii.m_fd, buf, ii.m_buf_capacity));
    LOG_trace (VR_INC (vr_trace_io)) << "  rc: " << rc;

    return rc;
}

VR_ASSUME_HOT signed_size_t
iterator::skip_callback (::archive * const a, void/*  iterator_impl */ * const ctx, int64_t const request)
{
    LOG_trace (vr_trace_io) << "skip_callback (" << a << ", " << ctx << ", " << request << ')';

    assert_nonnull (ctx);
    const iterator_impl & ii = * static_cast<const iterator_impl *> (ctx);

    assert_nonnegative (ii.m_fd);
    VR_CHECKED_SYS_CALL (::lseek64 (ii.m_fd, request, SEEK_CUR));

    return request;
}
//............................................................................
//............................................................................

VR_ASSUME_HOT signed_size_t
entry::read (int8_t * const buf) const
{
    check_nonnegative (m_size);
    assert_nonnull (buf);

    iterator::iterator_impl & ii = * m_parent.m_impl;

    signed_size_t const sz = (m_size > iterator::io_data_buf_size ? iterator::io_data_buf_size : m_size);

    ii.set_entry_buf (sz);
    VR_SCOPE_EXIT ([& ii]() { ii.reset_entry_buf (); });

    // use the convenience wrapper fn to avoid dealing with sparse entries:

    auto const rc = ::archive_read_data (ii.m_handle.get (), buf, m_size);


    if (VR_UNLIKELY (rc != m_size))
        throw_x (io_exception, "failed to read all " + string_cast (m_size) + " entry byte(s), rc = " + string_cast (rc));


    LOG_trace (VR_DEC (vr_trace_io)) << "entry '" + m_name + "': read " << rc << " bytes(s)";

    return rc;
}

} // end of 'tar'
//............................................................................
//............................................................................

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
