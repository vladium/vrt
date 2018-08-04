#pragma once

#include "vr/io/uri.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace rt
{
//............................................................................

// environment variables:

#if !defined (VR_ENV_DATA_ROOT)
#   define VR_ENV_DATA_ROOT         VR_APP_NAME "_DATA_ROOT"
#endif

//............................................................................

extern io::uri
resolve_as_uri (std::string const & path_or_name = { });

} // end of 'rt'
} // end of namespace
//----------------------------------------------------------------------------
