
#include "vr/io/files.h"
#include "vr/io/mapped_files.h"
#include "vr/sys/mem.h"
#include "vr/sys/os.h"
#include "vr/util/argparse.h"
#include "vr/util/logging.h"
#include "vr/utility.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

//----------------------------------------------------------------------------
namespace vr
{
//............................................................................

int32_t const g_page_size_mask { sys::os_info::instance ().page_size () - 1 };

void
run (fs::path const & path) // TODO recurse into dirs
{
    int32_t const fd = VR_CHECKED_SYS_CALL (::open (path.c_str (), (O_RDONLY | O_CLOEXEC)));
    auto _ = make_scope_exit ([fd]() { ::close (fd); });

    signed_size_t const extent = ((io::file_size (fd) + g_page_size_mask) & ~g_page_size_mask);

    addr_const_t const fd_mem = io::mmap_fd (nullptr, extent, PROT_READ, MAP_SHARED, fd, 0);
    auto __ = make_scope_exit ([fd_mem, extent]() { ::munmap (const_cast<addr_t> (fd_mem), extent); });

    {
        auto const fd_ih = sys::incore_histogram (fd_mem, extent);
        LOG_info << print (path) << ": " << print (fd_ih);
    }
    io::drop_from_cache (fd);
    {
        auto const fd_ih = sys::incore_histogram (fd_mem, extent);;
        LOG_info << print (path) << ": " << print (fd_ih);
    }
}

} // end of namespace
//----------------------------------------------------------------------------

VR_APP_MAIN ()
{
    using namespace vr;

    fs::path path { };

    bpopt::options_description opts { "usage: " + sys::proc_name () + " [options]" };
    opts.add_options ()
        ("input,i",     bpopt::value (& path)->value_name ("PATH")->required (), "input path")

        ("help,h",  "print usage information")
        ("version", "print build version")
    ;

    bpopt::positional_options_description popts { };
    popts.add ("input", 1); // alias 1st positional arg to 'input'

    int32_t rc { };
    try
    {
        VR_ARGPARSE (ac, av, opts, popts);

        run (path);
    }
    catch (std::exception const & e)
    {
        rc = 2;
        LOG_error << exc_info (e);
    }

    return rc;
}
//----------------------------------------------------------------------------
