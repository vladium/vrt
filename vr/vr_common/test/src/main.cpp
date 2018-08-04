
#include "vr/test/utility.h"
#include "vr/version.h"

//----------------------------------------------------------------------------

VR_APP_MAIN ()
{
    gt::InitGoogleTest (& ac, av);
    gt::AddGlobalTestEnvironment (new vr::test::env { });

    return RUN_ALL_TESTS ();
}
//----------------------------------------------------------------------------
