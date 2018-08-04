
#include "vr/io/resizable_buffer.h"

#include "vr/util/logging.h"
#include "vr/util/memory.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
//............................................................................

resizable_buffer::resizable_buffer (size_type const initial_capacity) :
    m_buf { boost::make_unique_noinit<int8_t []> (initial_capacity) },
    m_capacity { [initial_capacity]() { check_nonnegative (initial_capacity); return initial_capacity; } () }
{
}
//............................................................................

int8_t *
resizable_buffer::increase_capacity (size_type const capacity)
{
    // assertion: 'capacity' is positive

    // TODO shrinking strategy, thresholds, etc

    DLOG_trace1 << "increasing buffer capacity " << m_capacity << " -> " << capacity;

    m_buf = boost::make_unique_noinit<int8_t []> (capacity);
    m_capacity = capacity;

    return m_buf.get ();
}

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
