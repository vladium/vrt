#pragma once

#include "vr/macros.h"

VR_DIAGNOSTIC_PUSH ()
VR_DIAGNOSTIC_IGNORE ("-Wdeprecated-declarations")
#   include <boost/serialization/strong_typedef.hpp>
VR_DIAGNOSTIC_POP ()

//----------------------------------------------------------------------------

#define VR_STRONG_ALIAS(name, type) BOOST_STRONG_TYPEDEF (type, name)

//----------------------------------------------------------------------------
