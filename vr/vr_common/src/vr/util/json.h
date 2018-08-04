#pragma once

#include "vr/util/json_fwd.h"

#include <nlohmann/json.hpp>

//----------------------------------------------------------------------------
namespace vr
{
//............................................................................

template<>
inline std::string
print<json> (json const & j)
{
    return j.dump (4);
}
//............................................................................

#define VR_JSON_VALUE_TYPE_SEQ  \
    (object)                    \
    (array)                     \
    (string)                    \
    (boolean)                   \
    (number_integer)            \
    (number_unsigned)           \
    (number_float)              \
    (null)                      \
    /* */

/**
 * @note there is also json::type_name()
 */
template<>
std::string
print<json::value_t> (json::value_t const & type);

} // end of namespace
//----------------------------------------------------------------------------
