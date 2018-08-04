#pragma once

#include "vr/arg_map.h"
#include "vr/market/books/asx/pool_arenas.h"
#include "vr/market/sources/asx/ouch/OUCH_visitor.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
namespace ASX
{
class execution_manager; // forward
/**
 */
template<typename EXECUTION_BOOK>
class execution_view: noncopyable
{
    private: // ..............................................................

        using this_type         = execution_view<EXECUTION_BOOK>;

    public: // ...............................................................

        using book_type         = EXECUTION_BOOK;
        using src_traits        = source_traits<source::ASX>;

        execution_view (/* args to set initial capacity */) :
            m_pool_arena { /* TODO scale capacity */ },
            m_book { m_pool_arena }
        {
        }

        // ACCESSORs:

        EXECUTION_BOOK const & book () const
        {
            return m_book;
        }

    private: // ..............................................................

        friend class execution_manager; // grant mutator access

        using pool_arena_impl   = impl::execution_view_pool_arena<typename book_type::traits::pool_arena>;

        EXECUTION_BOOK & book ()
        {
            return m_book;
        }

        pool_arena_impl m_pool_arena;
        EXECUTION_BOOK m_book;

}; // end of class

} // end of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
