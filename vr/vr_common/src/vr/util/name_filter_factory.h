#pragma once

#include "vr/strings.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
/**
 */
class name_filter_factory: noncopyable
{
    public: // ...............................................................

        static name_filter create (std::string const & spec);

}; // end of class

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
