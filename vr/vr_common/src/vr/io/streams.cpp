
#include "vr/io/streams.h"

#include <boost/io/ios_state.hpp>

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
//............................................................................

int64_t
istream_length (std::istream & is)
{
    boost::io::ios_all_saver _ { is };

    setup_istream (is, no_exceptions { });
    auto const pos = is.tellg (); // save current r position

    is.seekg (0, is.end);
    auto const r = is.tellg ();

    is.seekg (pos); // restore r position (no boost io saver for that)

    return r;
}

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
