
#include "vr/market/sources/mock/defs.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
//............................................................................

vr_static_assert (std::is_move_constructible<client_connection>::value);
vr_static_assert (std::is_move_assignable<client_connection>::value);

//............................................................................

void
client_connection::close () VR_NOEXCEPT
{
    if (m_state != state::closed)
    {
        m_state = state::closed;

        try
        {
            m_data_link.reset ();
        }
        catch (...) { }
    }
}

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
