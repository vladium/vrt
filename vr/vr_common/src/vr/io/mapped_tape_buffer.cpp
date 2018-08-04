
#include "vr/io/mapped_tape_buffer.h"

#include "vr/io/files.h"
#include "vr/io/mapped_files.h" // mmap_fd()
#include "vr/sys/os.h"
#include "vr/utility.h"
#include "vr/util/logging.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
//............................................................................
/*
 * round 'requested' up to a page size multiple
 */
inline pos_t
select_size (pos_t const requested)
{
    check_positive (requested);

    auto const page_size = sys::os_info::instance ().page_size ();

    return (((requested + page_size - 1) / page_size) * page_size);
}
//............................................................................

mapped_tape_buffer::mapped_tape_buffer (fs::path const & file, pos_t const reserve_size, clobber::enum_t const cm) :
    m_reserve_size { select_size (reserve_size)  }
{
    fs::file_status const fstats = fs::status (file);
    bool const file_exists = fs::exists (fstats);
    check_condition (! file_exists || fs::is_regular (fstats), file);

    io::check_ostream_mode (file, cm, file_exists); // this may throw on cm 'retain/error'

    m_fd = VR_CHECKED_SYS_CALL (::open (file.c_str (), (O_CREAT | O_TRUNC | O_RDWR | O_CLOEXEC | O_NOATIME), ((S_IRUSR | S_IWUSR) | S_IRGRP)));
    assert_zero (io::file_size (m_fd)); // if existed, should have been truncated

    VR_CHECKED_SYS_CALL (::ftruncate (m_fd, m_reserve_size));

    VR_CHECKED_SYS_CALL_noexcept (::posix_fadvise (m_fd, 0, /* until eof */0, POSIX_FADV_SEQUENTIAL));

    LOG_trace2 << this << ": mmapping fd " << m_fd << " ...";

    m_base = io::mmap_fd (nullptr, m_reserve_size, (PROT_READ | PROT_WRITE), MAP_SHARED, m_fd, 0);
    m_r_addr = m_w_addr = m_base;

    LOG_trace1 << this << ": configured with reserve size " << m_reserve_size << " byte(s), base @" << static_cast<addr_t> (m_base);
}

mapped_tape_buffer::mapped_tape_buffer (arg_map const & args) :
    mapped_tape_buffer (args.get<fs::path> ("file"), args.get<pos_t> ("reserve_size", default_mmap_reserve_size ()),
                        args.get<clobber> ("cm", clobber::error))
{
}

mapped_tape_buffer::mapped_tape_buffer (mapped_tape_buffer && rhs) :
    m_reserve_size { rhs.m_reserve_size },
    m_base { rhs.m_base },
    m_r_addr { rhs.m_r_addr },
    m_w_addr { rhs.m_w_addr },
    m_fd { rhs.m_fd }
{
    rhs.m_base = nullptr; // grab ownership of the mapping
    rhs.m_fd = -1; // grab ownership of the file/fd
}

mapped_tape_buffer::~mapped_tape_buffer () VR_NOEXCEPT
{
    LOG_trace1 << this << ": on destruction size " << size () << ", capacity " << capacity ();

    truncate_and_close ();
}
//............................................................................

void
mapped_tape_buffer::truncate_and_close () VR_NOEXCEPT_IF (VR_RELEASE)
{
    addr_t base = m_base;
    int32_t const fd = m_fd;

    if (fd >= 0) // we own a file descriptor and possibly a mapping
    {
        m_base = nullptr;
        m_fd = -1;

        if (base)
        {
            // truncate 'fd' to the number of bytes r-committed:

            pos_t const r_count = intptr (m_r_addr) - intptr (base);
            assert_nonnegative (r_count);

            LOG_trace1 << this << ": truncating to " << r_count;

            VR_CHECKED_SYS_CALL_noexcept (::ftruncate (fd, r_count));
            VR_CHECKED_SYS_CALL_noexcept (::munmap (base, m_reserve_size));
        }

        VR_CHECKED_SYS_CALL_noexcept (::close (fd));
    }
}


} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
