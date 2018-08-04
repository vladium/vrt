#pragma once

#include "vr/enums.h"

#include <boost/regex.hpp>

//----------------------------------------------------------------------------
namespace vr
{

extern boost::regex const &
pcap_regex ();

//............................................................................

extern boost::regex const &
itch_regex ();

extern boost::regex const &
glimpse_regex ();

extern boost::regex const &
ouch_regex ();

//............................................................................

extern boost::regex const &
recv_regex ();

extern boost::regex const &
send_regex ();

//............................................................................

VR_ENUM (cap_kind,
    (
        itch,
        ouch,
        glimpse
    ),
    printable, parsable

); // end of enum

} // end of namespace
//----------------------------------------------------------------------------
