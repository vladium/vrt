
#include "vr/io/mapped_ring_buffer.h"

#include "vr/io/files.h"
#include "vr/io/mapped_files.h" // mmap_fd()
#include "vr/sys/os.h"
#include "vr/util/ops_int.h"
#include "vr/utility.h" // VR_SCOPE_EXIT

#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
//............................................................................
//............................................................................
namespace
{

using ops_checked       = util::ops_int<util::arg_policy<util::zero_arg_policy::ignore, 0>, true>;

/*
 * round 'requested' up to a power-of-2 page size multiple
 */
inline mapped_ring_buffer::size_type
select_capacity (mapped_ring_buffer::size_type const requested)
{
    check_positive (requested);

    auto const page_size = sys::os_info::instance ().page_size ();

    mapped_ring_buffer::size_type const requested_pages_ceil = (((requested + page_size - 1) / page_size) * page_size);

    return (1 << ops_checked::log2_ceil (requested_pages_ceil));
}
//............................................................................

addr_t
mmap_contiguous (mapped_ring_buffer::size_type const capacity)
{
    VR_IF_DEBUG
    (
        auto const page_size = sys::os_info::instance ().page_size ();

        check_is_power_of_2 (capacity, capacity); // caller ensures
        check_zero (capacity % page_size, capacity); // caller ensures
    )

    std::pair<fs::path, int32_t> const devshm_file = io::open_unique_file ("/dev/shm/", "mapped_ring_buffer");
    VR_SCOPE_EXIT ([& devshm_file]() { ::close (devshm_file.second); });
    fs::remove (devshm_file.first); // unlink right away, since the pathname won't be needed again

    LOG_trace2 << "mmapping fd " << devshm_file.second << " " << print (devshm_file.first) << " ...";

    VR_CHECKED_SYS_CALL (::ftruncate (devshm_file.second, capacity));

    bool commit { false };

    int8_t * const base = static_cast<int8_t *> (io::mmap_fd (nullptr, capacity << 1, PROT_NONE, (MAP_ANONYMOUS | MAP_PRIVATE), -1, 0));
    VR_SCOPE_EXIT ([& commit, capacity, base]() { if (! commit) VR_CHECKED_SYS_CALL_noexcept (::munmap (base, capacity << 1)); });

    {
        int8_t * const addr_lo = static_cast<int8_t *> (io::mmap_fd (base, capacity, (PROT_READ | PROT_WRITE), (MAP_FIXED | MAP_SHARED), devshm_file.second, 0));
        check_eq (addr_lo, base);

        int8_t * const addr_hi = static_cast<int8_t *> (io::mmap_fd (base + capacity, capacity, (PROT_READ | PROT_WRITE), (MAP_FIXED | MAP_SHARED), devshm_file.second, 0));
        check_eq (addr_hi, base + capacity);
    }

    commit = true;
    return base;
}

} // end of anonymous
//............................................................................
//............................................................................

mapped_ring_buffer::mapped_ring_buffer (size_type const capacity) :
    m_capacity_mask { select_capacity (capacity) - 1 }
{
    check_ge (m_capacity_mask + 1, capacity); // selected capacity >= user-requested

    m_base = static_cast<int8_t *> (mmap_contiguous (m_capacity_mask + 1));

    LOG_trace1 << "configured with capacity " << (m_capacity_mask + 1) << " bytes, base @" << static_cast<addr_t> (m_base);
}

mapped_ring_buffer::mapped_ring_buffer (arg_map const & args) :
    mapped_ring_buffer (args.get<size_type> ("capacity"))
{
}

mapped_ring_buffer::mapped_ring_buffer (mapped_ring_buffer && rhs) :
    m_capacity_mask { rhs.m_capacity_mask },
    m_base { rhs.m_base },
    m_r_position { rhs.m_r_position },
    m_w_position { rhs.m_w_position }
{
    rhs.m_base = nullptr; // grab ownership of the mapping
}

mapped_ring_buffer::~mapped_ring_buffer () VR_NOEXCEPT
{
    addr_t const base = m_base;
    if (base)
    {
        LOG_trace1 << "unmapping buffer of capacity " << (m_capacity_mask + 1) << " bytes, base @" << static_cast<addr_t> (base);

        m_base = nullptr;
        VR_CHECKED_SYS_CALL_noexcept (::munmap (base, capacity () << 1));
    }
}
//............................................................................

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
