
#include "vr/io/mapped_files.h"

#include "vr/asserts.h"
#include "vr/io/files.h"
#include "vr/sys/os.h"

#include <algorithm>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
//............................................................................

// cache these in this TU:

static signed_size_t const  g_page_size      { sys::os_info::instance ().page_size () };

//............................................................................

VR_ASSUME_HOT addr_t
mmap_fd (addr_t const addr, signed_size_t const extent, bitset32_t const prot, bitset32_t const flags, int32_t const fd, signed_size_t const offset)
{
    assert_condition ((extent > 0) & (extent % sys::os_info::instance ().page_size () == 0), extent);
    // ['fd' can be negative]

    // TODO with enough caps, can avoid zero-init overhead:

    addr_t const r = ::mmap (addr, extent, prot, flags, fd, offset);

    if (VR_LIKELY (r != MAP_FAILED))
        return r;
    else
    {
        auto const e = errno;

        throw_x (sys_exception, "mmap (" + hex_string_cast (addr) + ", " + string_cast (extent) +
                                ", " + hex_string_cast (prot) + ", " + hex_string_cast (flags) +
                                ", " + string_cast (fd) + ", " + string_cast (offset) + ") failed with error " +
                                string_cast (e) + ": " + std::strerror (e));
    }
}
//............................................................................
//............................................................................
namespace impl
{

mapped_file::mapped_file (advice_dir::enum_t const advice) :
    m_advice { advice }
{
}

mapped_file::~mapped_file () VR_NOEXCEPT
{
    close ();
}
//............................................................................

void
mapped_file::close () VR_NOEXCEPT
{
    int32_t const fd = m_fd;

    if (fd >= 0) // we own a file descriptor and possibly a mapping
    {
        m_fd = -1;

        if (m_base_addr) VR_CHECKED_SYS_CALL_noexcept (::munmap (m_base_addr, m_extent));
        VR_CHECKED_SYS_CALL_noexcept (::close (fd));
    }
}

void
mapped_file::advise () VR_NOEXCEPT_IF (VR_RELEASE)
{
    if (m_advice == advice_dir::forward)
    {
        assert_nonnegative (m_fd);
        VR_CHECKED_SYS_CALL_noexcept (::posix_fadvise (m_fd, 0, /* until eof */0, POSIX_FADV_SEQUENTIAL));
    }
}
//............................................................................

addr_t
mapped_file::reposition (pos_t const position, window_t const window, bitset32_t const prot)
{
    LOG_trace1 << "  reposition (" << position << ", " << window << ')';

    assert_nonnegative (position);
    assert_positive (window);

    // [note: not using Linux-specific 'remap_file_pages()' because not supported on all file systems
    //  of interest; also, deprecated/made slower starting with kernel 4.0]

    addr_t base_addr = m_base_addr;

    if (VR_LIKELY (base_addr != nullptr)) // branch not taken only once after construction
    {
        m_base_addr = nullptr; // clear so that destructor does not attempt to unmap again
        VR_CHECKED_SYS_CALL (::munmap (base_addr, m_extent));
    }

    // check if we need a bigger extent:

    signed_size_t const page_size = g_page_size;

    window_t new_extent = page_size + (((position + window - 1) & ~(page_size - 1)) - (position & ~(page_size - 1)));
    new_extent = std::max (new_extent, m_extent);

    VR_IF_DEBUG (if (new_extent > m_extent) DLOG_trace1 << "    increasing extent to " << new_extent;)

    // TODO ::madvise() or ::[posix_]fadvise()? is the post-open() advice remembered on remaps?

    pos_t base_position;

    switch (m_advice)
    {
        case advice_dir::forward: // slide the mapping forward in file as much as possible
        {
            // in 'forward' mode the start of data range is made to be within the first mapped page

            base_position = (position & ~(page_size - 1));
        }
        break;

        case advice_dir::backward: // slide the mapping backward in file as much as possible (but don't cross zero)
        {
            // in 'backward' mode the end of data range is made to be within the last mapped page

            base_position = std::max<pos_t> (0, ((position + window - 1) & ~(page_size - 1)) - new_extent + page_size);
        }
        break;

        default: VR_ASSUME_UNREACHABLE ();

    } // end of switch

    base_addr = mmap_fd (base_addr, new_extent, prot, MAP_SHARED, m_fd, base_position); // TODO allow MAP_PRIVATE when appropriate
    LOG_trace2 << "    remapped to @" << base_addr << " with extent " << new_extent
               << " and file offset " << base_position << " (" << (base_position / page_size) << " page(s))" ;

    window_t const offset = (position - base_position);

    m_base_addr = static_cast<int8_t *> (base_addr);
    m_base_position = base_position;
    m_offset = offset;
    m_extent = new_extent; // may not have changed, but we're likely writing into this cache line anyway

    return (static_cast<int8_t *> (base_addr) + offset);
}

} // end of 'impl'
//............................................................................
//............................................................................

mapped_ifile::mapped_ifile (fs::path const & file, advice_dir::enum_t const advice) :
    super (advice)
{
    fs::file_status const fstats = fs::status (file);
    check_condition (fs::exists (fstats), file);
    check_condition (fs::is_regular (fstats), file);

    m_fd = VR_CHECKED_SYS_CALL (::open (file.c_str (), (O_RDONLY | O_CLOEXEC | O_NOATIME)));

    advise ();

    // we open the file at construction time, but mmap()ing 'm_base_addr' is delayed until
    // actual I/O is done: this avoids potentially redundant I/O costs in the situations
    // when the I/O is started at a positive offset (e.g. from the end of the file) and
    // avoids having to guess/default or ask the user for the initial window size

    m_size = io::file_size (m_fd);
}
//............................................................................
//............................................................................

mapped_ofile::mapped_ofile (fs::path const & file, pos_t const reserve_size, clobber::enum_t const cm, advice_dir::enum_t const advice) :
    super (advice)
{
    check_positive (reserve_size);

    fs::file_status const fstats = fs::status (file);
    bool const file_exists = fs::exists (fstats);
    check_condition (! file_exists || fs::is_regular (fstats), file);

    io::check_ostream_mode (file, cm, file_exists); // this may throw on cm 'retain/error'

    m_fd = VR_CHECKED_SYS_CALL (::open (file.c_str (), (O_CREAT | O_TRUNC | O_RDWR | O_CLOEXEC | O_NOATIME), ((S_IRUSR | S_IWUSR) | S_IRGRP)));
    assert_zero (io::file_size (m_fd)); // if existed, should have been truncated

    // we open the file at construction time, but mmap()ing 'm_base_addr' is delayed until
    // actual I/O is done: this avoids potentially redundant I/O costs in the situations
    // when the I/O is started at a positive offset (e.g. from the end of the file) and
    // avoids having to guess/default or ask the user for the initial window size

    // [leave 'm_size' at zero]

    VR_CHECKED_SYS_CALL (::ftruncate (m_fd, reserve_size));

    advise ();
}

mapped_ofile::~mapped_ofile () VR_NOEXCEPT
{
    try
    {
        truncate_and_close (m_size);
    }
    catch (std::exception const & e)
    {
        LOG_error << exc_info (e);
    }
}
//............................................................................

bool
mapped_ofile::truncate_and_close (pos_t const size)
{
    int32_t const fd = m_fd;

    if (fd < 0)
        return false;
    else
    {
        check_within_inclusive (size, m_size);

        VR_CHECKED_SYS_CALL (::ftruncate (fd, size));
        m_size = size;

        close ();
        return true;
    }
}

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
