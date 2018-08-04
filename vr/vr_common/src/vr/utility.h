#pragma once

#include "vr/preprocessor.h"
#include "vr/util/logging.h"

#include <boost/optional.hpp>

#include <iterator>
#include <utility>

//----------------------------------------------------------------------------
namespace vr
{
//............................................................................
/**
 * [c++17]
 */
template<typename T>
using optional                      = boost::optional<T>;

//............................................................................

#define VR_SCOPE_EXIT(...) auto VR_CAT (vr_se_, __LINE__) = make_scope_exit (__VA_ARGS__)

//............................................................................

template<typename F>
struct scope_exit final
{
    VR_FORCEINLINE scope_exit (F && f) :
        m_f { std::forward<F> (f) }
    {
    }

    VR_FORCEINLINE ~scope_exit ()
    {
        m_f ();
    }

    F m_f;

}; // end of class

template<typename F>
scope_exit<F>
make_scope_exit (F && f)
{
    return scope_exit<F> (std::forward<F> (f));
};
//............................................................................

template<typename DERIVED>
class instance_counter // TODO consider a trait for using an atomic counter
{
    public: // ...............................................................

        static int64_t const & instance_count ()
        {
            return s_count;
        }

    protected: // ............................................................

        instance_counter ()
        {
            LOG_trace3 << this << ": incrementing " << s_count;

            ++ s_count;
        }

        ~instance_counter ()
        {
            LOG_trace3 << this << ": decrementing " << s_count;

            -- s_count;
        }

        static int64_t s_count;

}; // end of class

template<typename DERIVED>
int64_t instance_counter<DERIVED>::s_count { };

//............................................................................

template<typename CONTAINER, typename F>
void
erase_if_second (CONTAINER & c, F && f)
{
    for (auto i = std::begin (c); i != std::end (c); )
    {
        if (f (i->second))
            i = c.erase (i);
        else
            ++ i;
    }
}

} // end of namespace
//----------------------------------------------------------------------------
