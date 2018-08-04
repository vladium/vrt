#pragma once

#if !defined (_GNU_SOURCE)
#   define _GNU_SOURCE
#endif

//----------------------------------------------------------------------------

// common prefix for environment variables and derived names (log filenames, etc):

#if !defined (VR_APP_NAME)
#	define VR_APP_NAME              "VR"
#endif

// environment variables:

#if !defined (VR_ENV_LOG_ROOT)
#	define VR_ENV_LOG_ROOT          VR_APP_NAME "_LOG_ROOT"
#endif

#if !defined (VR_ENV_SIG_HANDLER)
#	define VR_ENV_SIG_HANDLER       VR_APP_NAME "_SIG_HANDLER"
#endif

#if !defined (VR_ENV_SIG)
#   define VR_ENV_SIG               VR_APP_NAME "_SIG"
#endif

//----------------------------------------------------------------------------
