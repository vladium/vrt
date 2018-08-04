#pragma once

#include "vr/types.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
//............................................................................
//............................................................................
namespace impl
{

constexpr uint64_t to_fw_string8_impl (string_literal_t const start, std::size_t const size, std::size_t const i)
{
    return (i == size ? 0 : ((static_cast<uint64_t> (static_cast<uint8_t> (start [i])) << (i * 8)) | to_fw_string8_impl (start, size, i + 1)));
}

} // end of 'impl'
//............................................................................
//............................................................................
/**
 * @ref based on Scott Schurr's str_const https://akrzemi1.wordpress.com/2011/05/11/parsing-strings-at-compile-time-part-i/
 */
class str_const
{
    public: // ...............................................................

        template<std::size_t N>
        constexpr str_const (char const (& str) [N]) :
            m_start { str },
            m_size { N -/* ! */1 }
        {
            static_assert (N > 0, "not a string literal");
        }


        constexpr operator string_literal_t () const VR_NOEXCEPT    { return m_start; }
        constexpr std::size_t size () const VR_NOEXCEPT             { return m_size; }

        constexpr uint64_t to_fw_string8 () const VR_NOEXCEPT       { return impl::to_fw_string8_impl (m_start, m_size, 0); }

    private: // ..............................................................

        string_literal_t m_start;
        std::size_t m_size;

}; // end of constexpr class

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
