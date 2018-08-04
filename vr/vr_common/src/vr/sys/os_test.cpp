
#include "vr/sys/os.h"

#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace sys
{
//............................................................................

TEST (proc_name, sniff)
{
    std::string const & pn = proc_name (false);
    std::string const & pn_short = proc_name (true);

    LOG_trace1 << "proc_name: " << print (pn) << ", short form: " << print (pn_short);

    EXPECT_FALSE (pn.empty ());
    EXPECT_FALSE (pn_short.empty ());
}
//............................................................................

TEST (short_sleep_for, sniff)
{
    short_sleep_for (0);    // zero delay is ok
    short_sleep_for (-1);   // negative delay is a noop

    short_sleep_for (1000000000L / 2);
}

TEST (long_sleep_for, sniff)
{
    long_sleep_for (0);     // zero delay is ok
    long_sleep_for (-1);    // negative delay is a noop

    long_sleep_for ((5 * 1000000000L) / 2);
}

} // end of 'sys'
} // end of namespace
//----------------------------------------------------------------------------
