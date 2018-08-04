
#include "vr/util/name_filter_factory.h"

#include "vr/io/files.h" // read_lines()
#include "vr/util/parse.h" // split()

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
//............................................................................

name_filter
name_filter_factory::create (std::string const & spec)
{
    if (spec.size () > 1 && spec [0] == '@') // an "@-file"
        return set_name_filter { io::read_lines (spec.substr (1)) };
    else if (spec.find (',') != std::string::npos) // {comma, space}-delimited set of strings
        return set_name_filter { util::split (spec, ", ") };

    return regex_name_filter { spec }; // default to a regex
}

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
