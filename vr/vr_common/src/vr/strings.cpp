
#include "vr/strings.h"

#include "vr/asserts.h"

//----------------------------------------------------------------------------
namespace vr
{
//............................................................................

regex_name_filter::regex_name_filter (std::string const & regex) :
    m_re { regex }
{
}
//............................................................................

std::ostream &
operator<< (std::ostream & os, hexdump const & obj) VR_NOEXCEPT_IF (VR_RELEASE)
{
    if (VR_UNLIKELY (obj.m_data == nullptr))
        return os << "<null>";

    assert_nonnegative (obj.m_size);

    uint8_t const * VR_RESTRICT const data = static_cast<uint8_t const *> (obj.m_data);

    for (int32_t i = 0, i_limit = obj.m_size; i < i_limit; ++ i)
    {
        if (i) os << ' ';

        uint32_t const v = data [i]; // widen
        os << "0123456789ABCDEF" [(v >> 4) & 0x0F] << "0123456789ABCDEF" [v & 0x0F];
    }

    return os;
}

} // end of namespace
//----------------------------------------------------------------------------
