
#include "vr/io/mapped_window_buffer.h"

#include <algorithm>

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
//............................................................................

mapped_window_buffer::mapped_window_buffer (fs::path const & file, size_type const capacity, size_type const w_window, pos_t const reserve_size,
                                            clobber::enum_t const cm) :
    m_w_window { w_window },
    m_capacity { capacity },
    m_out { file, reserve_size, cm, advice_dir::forward }
{
    check_positive (w_window);
    check_ge (capacity, w_window);

    m_out_w = m_out.seek<false> (0, capacity + w_window);

    LOG_trace1 << this << ": constructed with initial/min capacity " << m_capacity << ", w_window " << m_w_window;
}

mapped_window_buffer::mapped_window_buffer (arg_map const & args) :
    mapped_window_buffer (args.get<fs::path> ("file"), args.get<size_type> ("capacity"), args.get<size_type> ("w_window"),
                          args.get<pos_t> ("reserve_size", default_mmap_reserve_size ()),
                          args.get<clobber> ("cm", clobber::error))
{
}

mapped_window_buffer::~mapped_window_buffer () VR_NOEXCEPT
{
    LOG_trace1 << this << ": on destruction size " << size () << ", capacity " << m_capacity;

    truncate_and_close ();
}
//............................................................................

void
mapped_window_buffer::truncate_and_close ()
{
    // truncate 'm_out' more tightly than its default policy: to the number of bytes r-committed
    // (which may be smaller than 'm_out.size ()')

    pos_t const r_count = m_out.size () + m_r_offset - m_capacity - m_w_window;
    assert_nonnegative (r_count);

    LOG_trace1 << this << ": implied size " << m_out.size () << ", truncating to " << r_count;

    m_out.truncate_and_close (r_count);
}
//............................................................................

addr_t
mapped_window_buffer::w_remap_advance (size_type const step)
{
    auto const sz = size ();
    DLOG_trace2 << "[size: " << sz << ", capacity: " << m_capacity << "] w_remap_advance (" << step << ')';

    assert_condition (! m_out.eof ());

    size_type const capacity = std::max (m_capacity, (m_w_offset = sz + step)); // note: never shrinks
    addr_t const out_w = m_out.advance<false> (m_r_offset, capacity + m_w_window);

    VR_IF_DEBUG
    (
        if (capacity > m_capacity)
            DLOG_trace1 << "  increasing capacity " << m_capacity << " -> " << capacity;
    )

    m_out_w = out_w;
    m_r_offset = 0;
    m_capacity = capacity;

    return addr_plus (out_w, sz + step);
}

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
