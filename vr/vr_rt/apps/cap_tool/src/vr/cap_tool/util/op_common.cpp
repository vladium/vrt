
#include "vr/cap_tool/util/op_common.h"

//----------------------------------------------------------------------------
namespace vr
{

boost::regex const &
pcap_regex ()
{
    static boost::regex const g_pcap_re { R"(.*pcap.*)" };

    return g_pcap_re;
}
//............................................................................

boost::regex const &
itch_regex ()
{
    static boost::regex const g_pcap_re { R"(.*mcast.*)" }; // odd choice, historical reasons

    return g_pcap_re;
}

boost::regex const &
glimpse_regex ()
{
    static boost::regex const g_pcap_re { R"(.*glimpse.*)" }; // TODO allow this to be a prefix only?

    return g_pcap_re;
}

boost::regex const &
ouch_regex ()
{
    static boost::regex const g_pcap_re { R"(.*ouch.*)" }; // TODO allow this to be a prefix only?

    return g_pcap_re;
}
//............................................................................

boost::regex const &
recv_regex ()
{
    static boost::regex const g_pcap_re { R"(.*recv.*)" };

    return g_pcap_re;
}

boost::regex const &
send_regex ()
{
    static boost::regex const g_pcap_re { R"(.*send.*)" };

    return g_pcap_re;
}

} // end of namespace
//----------------------------------------------------------------------------
