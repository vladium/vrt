#pragma once

#include "vr/enums.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{

struct currency final
{
    VR_NESTED_ENUM
    (
        (
            USD,
            AUD
        )
        ,
        printable, parsable
    );

}; // end of enum

} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
