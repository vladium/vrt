#pragma once

#include "vr/macros.h"

//----------------------------------------------------------------------------

#if !defined (VR_BUILD_SRC_VERSION)
#   define VR_BUILD_SRC_VERSION     private
#endif

#define VR_GCC_VERSION              VR_TO_STRING (__GNUC__) "." VR_TO_STRING (__GNUC_MINOR__) "." VR_TO_STRING (__GNUC_PATCHLEVEL__)

//............................................................................

#if VR_RELEASE
#   define VR_BUILD_VARIANT         "r"
#else
#   define VR_BUILD_VARIANT         "D"
#endif

#define VR_BUILD_VERSION            VR_BUILD_VARIANT "-" VR_TO_STRING (VR_BUILD_SRC_VERSION) "-" VR_GCC_VERSION

#define VR_APP_VERSION()            char const * app_version () { return VR_BUILD_VERSION; }

#define VR_APP_MAIN()               \
VR_APP_VERSION () \
\
int \
main (int ac, char * av []) \
/* */

//----------------------------------------------------------------------------
