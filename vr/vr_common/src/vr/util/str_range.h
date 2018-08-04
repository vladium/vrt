#pragma once

#include "vr/asserts.h"
#include "vr/str_hash.h"

#include <algorithm>

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{

struct str_range
{
    using size_type         = int32_t;

    char_const_ptr_t m_start;
    size_type m_size;


    bool empty () const
    {
        return (! m_size);
    }

    // interfacing with 'std::string':

    friend VR_ASSUME_HOT void assign (std::string & lhs, str_range const & rhs) VR_NOEXCEPT
    {
        lhs.assign (rhs.m_start, rhs.m_size);
    }

    /**
     * @return an 'std::string' of size 'm_size' with a *copy* of this range's data
     */
    std::string to_string () const
    {
        return { m_start, static_cast<std::string::size_type> (m_size) };
    }

    // FRIENDs (inlined below):

    friend VR_FORCEINLINE bool operator== (str_range const & lhs, str_range const & rhs) VR_NOEXCEPT;
    friend VR_FORCEINLINE bool operator< (str_range const & lhs, str_range const & rhs) VR_NOEXCEPT;

    friend VR_FORCEINLINE bool operator!= (str_range const & lhs, str_range const & rhs) VR_NOEXCEPT
    {
        return (! (lhs == rhs));
    }

    // heterogeneous comparators:

    friend VR_FORCEINLINE bool operator== (str_range const & lhs, string_literal_t const rhs) VR_NOEXCEPT;
    friend VR_FORCEINLINE bool operator< (str_range const & lhs, string_literal_t const rhs) VR_NOEXCEPT;
    friend VR_FORCEINLINE bool operator!= (str_range const & lhs, string_literal_t const rhs) VR_NOEXCEPT
    {
        return (! (lhs == rhs));
    }

    friend VR_FORCEINLINE bool operator== (string_literal_t const lhs, str_range const & rhs) VR_NOEXCEPT
    {
        return (rhs == lhs);
    }
    friend VR_FORCEINLINE bool operator< (string_literal_t const lhs, str_range const & rhs) VR_NOEXCEPT;
    friend VR_FORCEINLINE bool operator!= (string_literal_t const lhs, str_range const & rhs) VR_NOEXCEPT
    {
        return (! (lhs == rhs));
    }


    friend VR_FORCEINLINE std::size_t hash_value (str_range const & obj) VR_NOEXCEPT;

    friend std::ostream & operator<< (std::ostream & os, str_range const & obj) VR_NOEXCEPT
    {
        return os << __print__ (obj);
    }

    friend VR_ASSUME_COLD std::string __print__ (str_range const & obj) VR_NOEXCEPT;

}; // end of class
//............................................................................

vr_static_assert (std::is_pod<str_range>::value);

//............................................................................

struct str_range_equal: public std::binary_function<str_range, str_range, bool>
{
    bool operator() (str_range const & lhs, str_range const & rhs) const
    {
        assert_nonnull (lhs.m_start);
        assert_nonnull (rhs.m_start);

        if (lhs.m_size != rhs.m_size) return false;

        // [assertion: 'lhs' and 'rhs' have equal sizes]

        return (! std::memcmp (lhs.m_start, rhs.m_start, lhs.m_size));
    }

    bool operator() (str_range const & lhs, string_literal_t const rhs) const
    {
        assert_nonnull (lhs.m_start);
        assert_nonnull (rhs);

        str_range::size_type const rhs_size = std::strlen (rhs);

        if (lhs.m_size != rhs_size) return false;

        // [assertion: 'lhs' and 'rhs' have equal sizes]

        return (! std::memcmp (lhs.m_start, rhs, rhs_size));
    }

}; // end of functor

struct str_range_less: public std::binary_function<str_range, str_range, bool>
{
    bool operator() (str_range const & lhs, str_range const & rhs) const
    {
        assert_nonnull (lhs.m_start);
        assert_nonnull (rhs.m_start);

        auto const min_size = std::min (lhs.m_size, rhs.m_size);
        auto const cmp_rc = std::strncmp (lhs.m_start, rhs.m_start, min_size);

        return ((cmp_rc < 0) | ((cmp_rc == 0) & (lhs.m_size < rhs.m_size)));
    }

    bool operator() (string_literal_t const lhs, str_range const & rhs) const
    {
        assert_nonnull (lhs);
        assert_nonnull (rhs.m_start);

        str_range::size_type const lhs_size = std::strlen (lhs);
        auto const min_size = std::min (lhs_size, rhs.m_size);
        auto const cmp_rc = std::strncmp (lhs, rhs.m_start, min_size);

        return ((cmp_rc < 0) | ((cmp_rc == 0) & (lhs_size < rhs.m_size)));
    }

    bool operator() (str_range const & lhs, string_literal_t const rhs) const
    {
        assert_nonnull (lhs.m_start);
        assert_nonnull (rhs);

        str_range::size_type const rhs_size = std::strlen (rhs);
        auto const min_size = std::min (lhs.m_size, rhs_size);
        auto const cmp_rc = std::strncmp (lhs.m_start, rhs, min_size);

        return ((cmp_rc < 0) | ((cmp_rc == 0) & (lhs.m_size < rhs_size)));
    }

}; // end of functor

/*
 * NOTE: this MUST be consistent with 'c_str_hasher'
 */
struct str_range_hasher: public std::unary_function<str_range, std::size_t>
{
    std::size_t operator() (str_range const & obj) const
    {
        assert_nonnull (obj.m_start);

        return str_hash_32 (obj.m_start, obj.m_size);
    }

}; // end of functor
//............................................................................

inline bool
operator== (str_range const & lhs, str_range const & rhs) VR_NOEXCEPT
{
    return str_range_equal { }(lhs, rhs);
}

inline bool
operator< (str_range const & lhs, str_range const & rhs) VR_NOEXCEPT
{
    return str_range_less { }(lhs, rhs);
}

inline std::size_t
hash_value (str_range const & obj) VR_NOEXCEPT
{
    return str_range_hasher { }(obj);
}
//............................................................................

inline bool
operator== (str_range const & lhs, string_literal_t const rhs) VR_NOEXCEPT
{
    return str_range_equal { }(lhs, rhs);
}

inline bool
operator< (str_range const & lhs, string_literal_t const rhs) VR_NOEXCEPT
{
    return str_range_less { }(lhs, rhs);
}
//............................................................................

inline bool
operator< (string_literal_t const lhs, str_range const & rhs) VR_NOEXCEPT
{
    return str_range_less { }(lhs, rhs);
}
//............................................................................

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
