#pragma once

#include "vr/strings_fwd.h"

#include <nlohmann/json_fwd.hpp>

//----------------------------------------------------------------------------
namespace vr
{
using json          = nlohmann::json;

//............................................................................

template<>
std::string
print<json> (json const & j);

} // end of namespace
//----------------------------------------------------------------------------
