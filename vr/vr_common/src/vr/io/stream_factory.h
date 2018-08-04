#pragma once

#include "vr/arg_map.h"
#include "vr/filesystem.h"
#include "vr/io/uri.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
/**
 */
class stream_factory final: noncopyable
{
    public: // ...............................................................

        static std::unique_ptr<std::istream> open_input (fs::path const & file, arg_map const & parms = { });
        static std::unique_ptr<std::istream> open_input (uri const & url, arg_map const & parms = { });

}; // end of class

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
