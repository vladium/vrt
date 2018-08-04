#pragma once

#include "vr/macros.h"
#include "vr/types.h"

#include <boost/make_unique.hpp>

#include <memory>
#include <cstdlib>  // std::malloc(), std::free()

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
/*
 * [offlined to add logging without a global logging.h #include]
 *
 * @throws alloc_failure (and thus not VR_NOEXCEPT)
 */
void *
aligned_alloc (std::size_t const alignment, signed_size_t const alloc_size);

void
aligned_free (void * const obj) VR_NOEXCEPT;

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
