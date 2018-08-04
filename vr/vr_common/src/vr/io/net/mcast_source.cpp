
#include "vr/io/net/mcast_source.h"

#include "vr/io/net/utility.h"
#include "vr/util/parse.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
namespace net
{
//............................................................................

static std::string const PATTERN { "'src_ip->(grp_ip[:grp_port])+'" };

//............................................................................

mcast_source::mcast_source (std::string const & spec)
{
    int32_t sep_width = 2;
    auto sep_ix = spec.find ("->");
    if (VR_UNLIKELY (sep_ix == std::string::npos))
    {
        sep_ix = spec.find ("="); // backwards compatibility
        if (VR_UNLIKELY (sep_ix == std::string::npos))
            throw_x (invalid_input, "expected descriptor spec of the form " + PATTERN + ": " + print (spec));
        sep_width = 1;
    }

    m_source = spec.substr (0, sep_ix);

    string_vector const groups { util::split (spec.substr (sep_ix + sep_width), ", ") };
    if (VR_UNLIKELY (groups.empty ()))
        throw_x (invalid_input, "expected descriptor spec of the form " + PATTERN + ": " + print (spec));

    for (std::string const & ap_str : groups)
    {
       m_groups.push_back (net::parse_addr_and_port (ap_str));
    }
}
//............................................................................

std::string
mcast_source::native () const
{
    std::stringstream ss { };

    ss << m_source << "->";

    bool first = true;
    for (auto const & group_ap : m_groups)
    {
        if (first)
            first = false;
        else
            ss << ", ";

        ss << std::get<0> (group_ap);

        if (std::get<1> (group_ap) >= 0)
            ss << ':' << std::get<1> (group_ap);
    }

    return ss.str ();
}


} // end of 'net'
} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
