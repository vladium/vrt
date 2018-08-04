
#include "vr/io/net/utility.h"

#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
namespace net
{
//............................................................................

TEST (hostname, sniff)
{
    std::string const & hn = hostname (false);
    std::string const & hn_short = hostname (true);

    LOG_trace1 << "hostname: " << print (hn) << ", short form: " << print (hn_short);

    EXPECT_FALSE (hn.empty ());
    EXPECT_FALSE (hn_short.empty ());
}

} // end of 'net'
} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
