#pragma once

#include "vr/util/di/component.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
namespace ASX
{
class ouch_session; // forward

/**
 */
class ouch_shell final: public util::di::component
{
    public: // ...............................................................

        ouch_shell ();


        void run (std::string const & prompt);

    private: // ..............................................................

        ouch_session * m_session { };   // [dep]

}; // end of class

} // and of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
