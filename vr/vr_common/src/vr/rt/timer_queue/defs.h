#pragma once

#include "vr/mc/cache_aware.h"
#include "vr/mc/mc.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace rt
{

// TODO use futexes for use cases that require real sleeping

class timer
{
    public: // ...............................................................

        timer (timestamp_t const ts_utc) :
            m_ts_utc { ts_utc }
        {
        }


        // ACCESSORs:

        timestamp_t const & ts_utc () const
        {
            return m_ts_utc;
        }

        VR_FORCEINLINE bool expired () const
        {
            return mc::volatile_cast (static_cast<bool const &> (m_expired));
        }

        // MUTATORs:

        VR_FORCEINLINE void expire ()
        {
            mc::volatile_cast (static_cast<bool &> (m_expired)) = true;
        }

    private: // ..............................................................

        mc::cache_line_padded_field<bool> m_expired { false };
        timestamp_t const m_ts_utc;

}; // end of class

} // end of 'rt'
} // end of namespace
//----------------------------------------------------------------------------
