
#include "vr/io/uri.h"

#include "vr/io/files.h"

#include <boost/regex.hpp>

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
//............................................................................
//............................................................................
namespace
{

boost::regex const g_uri_parse_re { R"(^(([^:\/?#]+):)?(//([^\/?#]*))?([^?#]*)(\?([^#]*))?(#(.*))?)", boost::regex::extended };

VR_FORCEINLINE util::str_range
to_str_range (boost::smatch::const_reference const & m)
{
    return { & * m.first, static_cast<util::str_range::size_type> (m.second - m.first) };
}

} // end of anonymous
//............................................................................
//............................................................................
namespace impl
{

components_base::components_base (boost::smatch && sm) :
    m_sm { std::move (sm) }
{
}

} // end of 'impl'
//............................................................................
//............................................................................

util::str_range
uri::components::scheme () const
{
    return to_str_range (m_sm [2]);
}

util::str_range
uri::components::authority () const
{
    return to_str_range (m_sm [4]);
}

util::str_range
uri::components::path () const
{
    return to_str_range (m_sm [5]);
}

util::str_range
uri::components::query () const
{
    return to_str_range (m_sm [7]);
}

util::str_range
uri::components::fragment () const
{
    return to_str_range (m_sm [9]);
}
//............................................................................
//............................................................................

uri::uri (string_literal_t const s) :
    m_native  { s }
{
    validate ();
}

uri::uri (std::string const & s) :
    m_native { s }
{
    validate ();
}

uri::uri (std::string && s) :
    m_native { std::move (s) }
{
    validate ();
}

uri::uri (fs::path const & file) :
    uri ("file://" + io::weak_canonical_path (file).string ())
{
}
//............................................................................

inline void // internal linkage
uri::validate () const
{
    components const c = split ();
    std::string const & url = m_native; // for macro below

    check_condition (! c.scheme ().empty (), url);
    check_condition (! c.authority ().empty () || ! c.path ().empty (), url);
}
//............................................................................

uri::components
uri::split () const
{
    boost::smatch sm { };
    boost::regex_match (m_native, sm, g_uri_parse_re); // TODO check return

    return { std::move (sm) };
}
//............................................................................

//bool
//uri::is_valid (std::string const & s)
//{
//    // TODO make smth like this work: return boost::regex_match (s, g_uri_validate_re);
//
//    boost::cmatch cm { };
//    if (! boost::regex_match (s, cm, g_uri_parse_re)) return false;
//
//    // TODO check parts
//
//    return true;
//}
//............................................................................

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
