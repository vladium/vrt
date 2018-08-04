
#include "vr/sys/mem.h"

#include "vr/sys/os.h"
#include "vr/util/memory.h"

#include <unistd.h>
#include <sys/mman.h>

//----------------------------------------------------------------------------
namespace vr
{
namespace sys
{
//............................................................................

#define vr_incore_query(mem, extent) \
    check_nonnull (mem); \
    check_positive (extent); \
    \
    int32_t const log2_page_size = os_info::instance ().log2_page_size (); \
    signed_size_t const vec_size = ((extent + (1 << log2_page_size) - 1) >> log2_page_size); \
    assert_positive (vec_size); \
    \
    std::unique_ptr<uint8_t []> vec { boost::make_unique_noinit<uint8_t []> (vec_size) }; \
    uint8_t * VR_RESTRICT const vec_ = vec.get (); \
    \
    VR_CHECKED_SYS_CALL (::mincore (const_cast<addr_t> (mem), extent, vec_)) \
    /* */
//............................................................................

std::vector<double>
incore_histogram (addr_const_t const mem, std::size_t const extent, int32_t const bins)
{
    assert_positive (bins);

    vr_incore_query (mem, extent);

    std::vector<double> r (bins); // vector construction

    // TODO better accounting for 'bins' >> 'vec_size'

    double const d = 1.0 / vec_size;

    for (signed_size_t p = 0; p < vec_size; ++ p)
    {
        int32_t const bin = std::min<int32_t> ((p * bins) * d, bins - 1);
        if (vec_ [p] & 1) r [bin] += d;
    }

    return r;
}

bit_set
incore_page_map (addr_const_t const mem, std::size_t const extent)
{
    vr_incore_query (mem, extent);

    bit_set r { make_bit_set (vec_size) };
    for (signed_size_t p = 0; p < vec_size; ++ p)
        r.set (p, vec_ [p] & 1); // note: only least significant bit is defined

    return r;
}

} // end of 'sys'
} // end of namespace
//----------------------------------------------------------------------------
