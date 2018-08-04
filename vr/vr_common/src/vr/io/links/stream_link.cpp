
#include "vr/io/links/stream_link.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
//............................................................................
//............................................................................
namespace impl
{

stream_link_base::stream_link_base (std::istream & is) :
    m_s { & is }
{
}

stream_link_base::stream_link_base (std::ostream & os) :
    m_s { & os }
{
}

} // end of 'impl'
//............................................................................
//............................................................................
} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------