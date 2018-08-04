#pragma once

#include "vr/strings_fwd.h"

#include <boost/filesystem.hpp>
namespace fs    = boost::filesystem;

//----------------------------------------------------------------------------
namespace vr
{

template<>
inline std::string
print<fs::path> (fs::path const & p)
{
    return '[' + p.native () + ']';
}

} // end of namespace
//----------------------------------------------------------------------------
