
#include "vr/util/cal/calendar.h"



//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
//............................................................................

using date_set          = std::set<date_t>;
using iterator          = date_set::iterator;

struct calendar::pimpl final // TODO is there a non-stupid impl of 'weekends'?
{
    pimpl (settings const & cfg)
    {
        // TODO
    }

    VR_FORCEINLINE iterator begin () const
    {
        return m_set.begin ();
    }

    VR_FORCEINLINE iterator end () const
    {
        return m_set.end ();
    }

    VR_FORCEINLINE iterator find (rop::enum_t const op, date_t const & d) const
    {
        throw_x (invalid_input, "not implemented yet");
    }


    date_set const m_set;

}; // end of nested class
//............................................................................

calendar::calendar (settings const & cfg) :
    m_impl { std::make_unique<pimpl> (cfg) }
{
}

calendar::~calendar ()    = default; // pimpl
//............................................................................

iterator
calendar::begin () const
{
    assert_nonnull (m_impl);
    return m_impl->begin ();
}

iterator
calendar::end () const
{
    assert_nonnull (m_impl);
    return m_impl->end ();
}

// query:

iterator
calendar::find (rop::enum_t const op, date_t const & d) const
{
    assert_nonnull (m_impl);
    return m_impl->find (op, d);
}

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
