#pragma once

#include "vr/data/attributes.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace data
{
namespace parse
{
/**
 *
 */
class attr_parser final
{
    public: // ...............................................................

        static std::vector<data::attribute> parse (std::string const & s);


}; // end of class

} // end of 'parse'
} // end of 'data'
} // end of namespace
//----------------------------------------------------------------------------
