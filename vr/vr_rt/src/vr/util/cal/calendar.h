#pragma once

#include "vr/operators.h"
#include "vr/settings_fwd.h"
#include "vr/util/datetime.h"

#include <set>

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
/**
 */
class calendar final // move-copyable, move-assignable
{
    private: // ..............................................................

        using date_set          = std::set<date_t>;
        using iterator          = date_set::iterator;

    public: // ...............................................................

        calendar (settings const & cfg);
        ~calendar ();

        calendar (calendar && rhs)              = default;
        calendar & operator= (calendar && rhs)  = default;

        // iteration:

        iterator begin () const;
        iterator end () const;

        // query:

        bool contains (date_t const & d) const;
        iterator find (rop::enum_t const op, date_t const & d) const;

        // define min/max date range -> begin()/end(), rbegin()/rend()
        // use std::set<date_as_int>; AND/OR -> interset/union, NOT -> subtract from entire range?

        // find(rop::enum_t, date_t): iterator (forward)
        // can do std::make_reverse_iterator (i) to iterate backwards

        // overload logical ops for this class?

    private: // ..............................................................

        class pimpl; // forward

        std::unique_ptr<pimpl> m_impl;

}; // end of class

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
