
#include "vr/market/books/execution_order_book.h"

#include "vr/util/ops_int.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
namespace ex
{
//............................................................................

std::ostream &
operator<< (std::ostream & os, fsm_state const & obj) VR_NOEXCEPT
{
    os << print (obj.order_state ());

    auto ps = obj.pending_state ();
    if (ps)
    {
        os << " {";

        bool first { true };
        while (ps > 0)
        {
            int32_t const bit = (ps & - ps);
            ps ^= bit;

            if (first)
                first = false;
            else
                os << '|';

            os << static_cast<fsm_state::pending::enum_t> (bit);
        }

        os << '}';
    }

    return os;
}
//............................................................................

std::ostream &
operator<< (std::ostream & os, fsm_error const & obj) VR_NOEXCEPT
{
    if (VR_LIKELY (obj.m_lineno <= 0))
        return os << "N/A";
    else
        return os << "[line: " << obj.m_lineno << ", src state: " << print (obj.m_state) << ']';
}

} // end of 'ex'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
