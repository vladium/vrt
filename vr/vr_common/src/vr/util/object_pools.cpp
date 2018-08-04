
#include "vr/util/object_pools.h"

#include <cstring>

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
//............................................................................
//............................................................................
namespace impl
{

int64_t
fixed_size_allocator_base::create (int64_t const initial_capacity, std::size_t const storage_size, std::size_t const alignment, std::size_t const chunk_capacity)
{
    int64_t const chunk_count = (initial_capacity + chunk_capacity - 1) / chunk_capacity;
    assert_positive (chunk_count);

    m_chunks = std::make_unique<addr_t []> (chunk_count); // init to nulls

    for (int64_t chunk_index = 0; chunk_index < chunk_count; ++ chunk_index)
    {
        m_chunks [chunk_index] = util::aligned_alloc (alignment, storage_size * chunk_capacity);
        check_nonnull (m_chunks [chunk_index]);
    }

    LOG_trace1 << util::instance_name (this) << ": created " << chunk_count << " chunk(s) of capacity " << chunk_capacity << " (storage size: " << storage_size << ", alignment: " << alignment << ')';
    return chunk_count;
}

void
fixed_size_allocator_base::destruct (int64_t const chunk_count) VR_NOEXCEPT
{
    if (m_chunks) // just to be safe
    {
        LOG_trace1 << util::instance_name (this) << ": count of chunks at destruction time: " << chunk_count;

        for (int64_t chunk_index = 0; chunk_index < chunk_count; ++ chunk_index)
        {
            util::aligned_free (m_chunks [chunk_index]);
        }

        m_chunks.reset ();
    }
}
//............................................................................

addr_t
fixed_size_allocator_base::allocate_chunk (int64_t const chunk_count, std::size_t const alignment, std::size_t const chunk_size)
{
    LOG_trace1 << "  [chunks: " << chunk_count << "] adding new chunk of size " << chunk_size << " byte(s) (alignment: " << alignment << ')';

    addr_t const chunk = util::aligned_alloc (alignment, chunk_size); // throws on failure

    // for object pooling in steady state, it's ok to have linear capacity growth:

    std::unique_ptr<addr_t []> new_chunks { boost::make_unique_noinit<addr_t []> (chunk_count + 1) };

    // place the new chunk at the end of new chunk holder array (otherwise all outstanding refs
    // get invalidated):

    std::memcpy (new_chunks.get (), m_chunks.get (), chunk_count * sizeof (addr_t));
    new_chunks [chunk_count] = chunk;

    m_chunks.swap (new_chunks);

    return chunk;
}

} // end of 'impl'
//............................................................................
//............................................................................
} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
