
#include "vr/util/shared_object.h"

#include "vr/asserts.h" // note: moving this include causes cyclic inclusion problems

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
//............................................................................
//............................................................................
namespace impl
{

void *
allocate_storage (int32_t const size, std::size_t const alignment, signed_size_t const alloc_size)
{
    check_positive (size, alignment, alloc_size);

    return util::aligned_alloc (alignment, alloc_size); // throws on alloc failure
}

} // end of 'impl'
//............................................................................
//............................................................................
} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
