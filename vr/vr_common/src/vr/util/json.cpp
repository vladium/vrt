
#include "vr/macros.h"
#include "vr/util/json.h"

#include <boost/preprocessor/seq/for_each.hpp>

//----------------------------------------------------------------------------
namespace vr
{

template<>
std::string
print<json::value_t> (json::value_t const & type)
{
    switch (type)
    {
#   define vr_CASE(r, unused, TYPE) \
        case json::value_t:: TYPE :   return VR_TO_STRING (TYPE); \
        /* */

        BOOST_PP_SEQ_FOR_EACH (vr_CASE, unused, VR_JSON_VALUE_TYPE_SEQ)

#   undef vr_CASE

        default: return "null"; // silent compiler warning, same in the parser code

    } // end of switch
}

} // end of namespace
//----------------------------------------------------------------------------
