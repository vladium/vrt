
#include "vr/util/memory.h"

#include "vr/asserts.h"
#include "vr/exceptions.h"
#include "vr/util/logging.h"
#include "vr/util/type_traits.h"

#include <boost/align/aligned_alloc.hpp>

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{

void *
aligned_alloc (std::size_t const alignment, signed_size_t const alloc_size)
{
    LOG_trace1 << "allocating " << alloc_size << " byte(s) ...";

    void * const mem = boost::alignment::aligned_alloc (alignment, alloc_size);
    if (VR_UNLIKELY (mem == nullptr))
    {
        LOG_fatal << "failed to allocate " << alloc_size << " byte(s), alignment " << alignment;

        throw_x (alloc_failure, "failed to allocate " + string_cast (alloc_size) + " byte(s)");
    }

    assert_zero (uintptr (mem) % alignment, alignment);

    return mem;
}

void
aligned_free (void * const obj) VR_NOEXCEPT
{
    boost::alignment::aligned_free (obj);
}

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
