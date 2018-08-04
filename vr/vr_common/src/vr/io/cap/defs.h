#pragma once

#include "vr/enums.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
//............................................................................

VR_ENUM (cap_format,
    (
        wire,       // records as sent/received by a link (no separating record headers)
        pcap
    ),
    printable, parsable

); // end of enum

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
