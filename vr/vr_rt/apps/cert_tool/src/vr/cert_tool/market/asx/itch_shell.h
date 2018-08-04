#pragma once

#include "vr/util/di/component.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
namespace ASX
{
class itch_session; // forward

/**
 */
class itch_shell final: public util::di::component
{
    public: // ...............................................................

        itch_shell ();


        void run (std::string const & prompt);

    private: // ..............................................................

        itch_session * m_session { };   // [dep]

}; // end of class

} // and of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
