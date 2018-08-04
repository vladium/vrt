#pragma once

#include "vr/mc/cache_aware.h"
#include "vr/mc/mc.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace mc
{
/**
 * a convenience wrapper around 'cache_line_padded_field<bool>' that does the necessary
 * volatile_cast'ing in access/mutation
 */
template<bool CACHE_LINE_ALIGN = true>
class spinflag final: private cache_line_padded_field<int32_t>
{
    private: // ..............................................................

        using super         = cache_line_padded_field<int32_t>;

    public: // ...............................................................

        spinflag () :
            super { false }
        {
        }


        // ACCESSORs:

        VR_FORCEINLINE bool is_raised () const
        {
            return volatile_cast (static_cast<int32_t const &> (* this));
        }

        // MUTATORs:

        VR_ASSUME_COLD void raise ()
        {
            volatile_cast (static_cast<int32_t &> (* this)) = true;
        }

}; // end of master

template<>
class spinflag</* CACHE_LINE_ALIGN */false> final
{
    public: // ...............................................................

        spinflag ()     = default;


        // ACCESSORs:

        VR_FORCEINLINE bool is_raised () const
        {
            return volatile_cast (m_flag);
        }

        // MUTATORs:

        VR_ASSUME_COLD void raise ()
        {
            volatile_cast (m_flag) = true;
        }

    private: // ..............................................................

        int32_t m_flag { false };

}; // end of specialization

} // end of 'mc'
} // end of namespace
//----------------------------------------------------------------------------
