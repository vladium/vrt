#pragma once

#include "vr/strings.h"
#include "vr/util/ops_int.h"
#include "vr/util/str_range.h"

//----------------------------------------------------------------------------
namespace vr
{
template<std::size_t N, bool CHECK_BOUNDS>
class fw_string; // forward

//............................................................................
//............................................................................
namespace impl
{

template<typename ST>
struct fw_string_impl_nochecks
{
    static constexpr int32_t max_size () { return sizeof (ST); }

    /*
     * return 'true' if successful ('str' fits into 'ST' storage)
     */
    static VR_FORCEINLINE bool copy_string_literal (string_literal_t const str, ST & data) VR_NOEXCEPT
    {
        data = { }; // make sure the whole storage slot had deterministic contents (for efficient comparisons, hashing, etc)

        std::string::value_type * const VR_RESTRICT data_chars = reinterpret_cast<std::string::value_type *> (& data);

        int32_t i;
        for (i = 0; i < max_size (); ++ i) // let the compiler know this loop length is bounded by a small value
        {
            if (! (data_chars [i] = str [i]))
                break;
        }

        // regardless of how the loop above exited, 'str' fits iff 'str [i]' is its null terminator:

        return (! str [i]);
    }

    /*
     * return 'true' if successful ('[str, str + len)' fits into 'ST' storage)
     */
    template<typename SIZE_T>
    static VR_FORCEINLINE bool copy_string_range (char_const_ptr_t const str, SIZE_T const len, ST & data) VR_NOEXCEPT
    {
        if (! vr_is_within_inclusive (len, max_size ())) return false;

        data = { }; // make sure the whole storage slot had deterministic contents (for efficient comparisons, hashing, etc)

        std::string::value_type * const VR_RESTRICT data_chars = reinterpret_cast<std::string::value_type *> (& data);

        std::memcpy (data_chars, str, len); // TODO more efficient impl (no need to loop)

        return true;
    }

}; // end of class
//............................................................................

template<typename ST, bool CHECK_BOUNDS>
struct fw_string_impl
{
    using impl_ops_nochecks         = fw_string_impl_nochecks<ST>;

    static constexpr int32_t max_size () { return impl_ops_nochecks::max_size (); }


    static VR_FORCEINLINE void copy_string_literal (string_literal_t const str, ST & data)
    {
        bool const rc = impl_ops_nochecks::copy_string_literal (str, data);

        if (CHECK_BOUNDS) // compile-time guard
        {
            if (VR_UNLIKELY (! rc)) throw_x (::vr::out_of_bounds, "failed: expected str of length no greater than " + string_cast (max_size ())); // TODO compile-time str_const
        }
    }

    template<typename SIZE_T>
    static VR_FORCEINLINE void copy_string_range (char_const_ptr_t const str, SIZE_T const len, ST & data)
    {
        bool const rc = impl_ops_nochecks::copy_string_range (str, len, data);

        if (CHECK_BOUNDS) // compile-time guard
        {
            if (VR_UNLIKELY (! rc)) throw_x (::vr::out_of_bounds, "failed: expected str range of size no greater than " + string_cast (max_size ())); // TODO compile-time str_const
        }
    }

}; // end of class
//............................................................................

template<std::size_t LHS_N, std::size_t RHS_N, bool CHECK_BOUNDS>
struct fw_string_ops; // forward


template<std::size_t N, bool CHECK_BOUNDS>
struct fw_string_heterogeneous_ops
{
    using lhs_type                  = fw_string<N, CHECK_BOUNDS>;
    using storage_type              = typename lhs_type::storage_type;
    using impl_ops_nochecks         = fw_string_impl_nochecks<storage_type>;

    // forwards:

    static VR_FORCEINLINE bool equal (lhs_type const & lhs, string_literal_t const rhs) VR_NOEXCEPT;
    static VR_FORCEINLINE bool equal (lhs_type const & lhs, util::str_range const & rhs) VR_NOEXCEPT;

}; // end of class

} // end of 'impl'
//............................................................................
//............................................................................
/**
 * @note this impl is for LE architecture only
 */
template<std::size_t N, bool CHECK_BOUNDS = VR_CHECK_INPUT>
class fw_string // ordered, hashable, copy/move-constructible, copy/move-assignable
{
    public: // ...............................................................

        using storage_type          = typename util::unsigned_type_of_size<N>::type;

    private: // ..............................................................

        using impl_ops              = impl::fw_string_impl<storage_type, CHECK_BOUNDS>;

    public: // ...............................................................

        using value_type            = std::string::value_type;
        using size_type             = int32_t;

        using reference             = std::string::reference;
        using const_reference       = std::string::const_reference;


        static constexpr size_type max_size () { return N; }
        static constexpr bool bounds_checked () { return CHECK_BOUNDS; }


        fw_string () :
            m_data { }
        {
        }

        explicit fw_string (storage_type const data) :
            m_data { data }
        {
        }

        fw_string (string_literal_t const str)
        {
            impl_ops::copy_string_literal (str, m_data);
        }

        template<typename SIZE_T>
        fw_string (char_const_ptr_t const str, SIZE_T const len)
        {
            impl_ops::copy_string_range (str, len, m_data);
        }

        fw_string (util::str_range const & s)
        {
            impl_ops::copy_string_range (s.m_start, s.m_size, m_data);
        }

        fw_string (std::string const & s)
        {
            impl_ops::copy_string_range (s.data (), s.size (), m_data);
        }


        /**
         * @return an 'std::string' of size 'size()' with a copy of this string's data
         */
        std::string to_string () const VR_NOEXCEPT
        {
            return { data (), static_cast<std::string::size_type> (size ()) };
        }

        // ACCESSORs:

        size_type size () const VR_NOEXCEPT
        {
            return util::ls_zero_index (m_data);
        }

        size_type length () const VR_NOEXCEPT
        {
            return size ();
        }

        bool empty () const VR_NOEXCEPT
        {
            return (! size ());
        }

        /**
         * @note unlike c++17 std::string, this is not guaranteed to be null-terminated
         */
        value_type const * data () const VR_NOEXCEPT
        {
            return reinterpret_cast<value_type const *> (& m_data);
        }

        const_reference const & at (size_type const index) const
        {
            if (CHECK_BOUNDS) check_within (index, size ());

            return data ()[index];
        }

        /**
         * @note unlike @ref at(), this never checks 'index' bounds (regardless of 'CHECK_BOUNDS')
         */
        template<typename IX, typename _ = IX>
        typename std::enable_if<std::is_integral<_>::value, const_reference>::type
        operator[] (IX const index) const VR_NOEXCEPT
        {
            return data ()[index];
        }

        // MUTATORs:

        /**
         * @note unlike c++17 std::string, this is not guaranteed to be null-terminated
         */
        value_type * data () VR_NOEXCEPT
        {
            return const_cast<value_type *> (const_cast<fw_string const *> (this)->data ());
        }

        reference const & at (size_type const index)
        {
            if (CHECK_BOUNDS) check_within (index, size ());

            return data ()[index];
        }

        /**
         * @note unlike @ref at(), this never checks 'index' bounds (regardless of 'CHECK_BOUNDS')
         */
        template<typename IX, typename _ = IX>
        typename std::enable_if<std::is_integral<_>::value, reference>::type
        operator[] (IX const index) VR_NOEXCEPT
        {
            return data ()[index];
        }


        // FRIENDs/OPERATORs:

        // [operators ==, <, !=, defined below]

        // TODO comparison with std::string, str_range, string_literal_t, etc operands

        // heterogeneous comparators:

        friend VR_FORCEINLINE bool operator== (fw_string const & lhs, string_literal_t const rhs) VR_NOEXCEPT
        {
            return impl::fw_string_heterogeneous_ops<N, CHECK_BOUNDS>::equal (lhs, rhs);
        }
        friend VR_FORCEINLINE bool operator== (string_literal_t const lhs, fw_string const & rhs) VR_NOEXCEPT
        {
            return (rhs == lhs);
        }

        friend VR_FORCEINLINE bool operator== (fw_string const & lhs, util::str_range const & rhs) VR_NOEXCEPT
        {
            return impl::fw_string_heterogeneous_ops<N, CHECK_BOUNDS>::equal (lhs, rhs);
        }
        friend VR_FORCEINLINE bool operator== (util::str_range const & lhs, fw_string const & rhs) VR_NOEXCEPT
        {
            return (rhs == lhs);
        }

        VR_FORCEINLINE explicit operator storage_type () const
        {
            return m_data;
        }

        friend VR_FORCEINLINE std::size_t hash_value (fw_string const & obj) VR_NOEXCEPT
        {
            return util::i_crc32 (1, obj.m_data);
        }

        friend VR_ASSUME_COLD std::string __print__ (fw_string const & obj) VR_NOEXCEPT
        {
            return quote_string<'"'> (obj.to_string ());
        }

        friend std::ostream & operator<< (std::ostream & os, fw_string const & obj) VR_NOEXCEPT
        {
            os.write (obj.data (), obj.size ());
            return os;
        }

    private: // ..............................................................

        template<std::size_t, std::size_t, bool>
        friend struct impl::fw_string_ops; // grant access to 'm_data'

        template<std::size_t, bool>
        friend struct impl::fw_string_heterogeneous_ops; // grant access to 'm_data'


        storage_type m_data;

}; // end of class
//............................................................................

using fw_string4            = fw_string<4>;
using fw_string8            = fw_string<8>;

//............................................................................
//............................................................................
namespace impl
{
/*
 * note: I could just use '(lhs.m_data < rhs.m_data)' if I just wanted an ordered type;
 * instead, the impl below does lexicographic comparison (std:strcmp()-like)
 * <em>as long as positive chars are used</em>:
 */
template<typename T, typename = void>
struct fw_string_lexless; // end of master

// TODO pshufb ?

template<typename T>
struct fw_string_lexless<T,
    typename std::enable_if<(sizeof (T) == sizeof (int32_t))>::type>
{
    static VR_FORCEINLINE bool evaluate (int32_t const lhs, int32_t const rhs) VR_NOEXCEPT
    {
        xmmi_t temp_lhs, temp_rhs;
        int32_t r { }; // note: zero init (hence the '&' modifier for %0 below)
        asm
        (
            "movd %3, %1\n"
            "movd %4, %2\n"
            "pcmpistri %5, %1, %2\n"
            "jnc     %=f\n"
            "notl  %%ecx\n"
            "addl $4, %%ecx\n" // ecx <- 3 - ecx (note: '4' is 'sizeof(T)')
            "shll $3, %%ecx\n"
            "pcmpgtb %1, %2\n" // (note: *signed* byte comparisons)
            "movd %2,  %0\n"
            "shll %%cl, %0\n"
        "%=:            \n"
                : "+&r" (r), "=x" (temp_lhs), "=x" (temp_rhs)
                : "rm" (lhs), "rm" (rhs),
                  "N" (_SIDD_CMP_EQUAL_EACH | _SIDD_NEGATIVE_POLARITY) // looking for the first diff byte
                : "%ecx", "cc"
        );

        return r;
    }

}; // end of specialization

template<typename T>
struct fw_string_lexless<T,
    typename std::enable_if<(sizeof (T) == sizeof (int64_t))>::type>
{
    static VR_FORCEINLINE bool evaluate (int64_t const lhs, int64_t const rhs) VR_NOEXCEPT
    {
        xmmi_t temp_lhs, temp_rhs;
        int64_t r { }; // note: zero init (hence the '&' modifier for %0 below)
        asm
        (
            "movq %3, %1\n"
            "movq %4, %2\n"
            "pcmpistri %5, %1, %2\n"
            "jnc     %=f\n"
            "notl  %%ecx\n"
            "addl $8, %%ecx\n" // ecx <- 7 - ecx (note: '8' is 'sizeof(T)')
            "shll $3, %%ecx\n"
            "pcmpgtb %1, %2\n" // (note: *signed* byte comparisons)
            "movq %2,  %0\n"
            "shlq %%cl, %0\n"
        "%=:            \n"
                : "+&r" (r), "=x" (temp_lhs), "=x" (temp_rhs)
                : "rm" (lhs), "rm" (rhs),
                  "N" (_SIDD_CMP_EQUAL_EACH | _SIDD_NEGATIVE_POLARITY) // looking for the first diff byte
                : "%ecx", "cc"
        );

        return r;
    }

}; // end of specialization
//............................................................................

// 'fw_string_ops' impl:

template<std::size_t LHS_N, std::size_t RHS_N, bool CHECK_BOUNDS>
struct fw_string_ops
{
    static VR_FORCEINLINE bool
    equal (fw_string<LHS_N, CHECK_BOUNDS> const & lhs, fw_string<RHS_N, CHECK_BOUNDS> const & rhs) VR_NOEXCEPT
    {
        return (lhs.m_data == rhs.m_data); // correct because bytes after the null terminator are also nulls (and/or will be zero-extended)
    }


    template<std::size_t _ = LHS_N>
    static VR_FORCEINLINE typename std::enable_if<(_ != RHS_N), bool>::type
    less (fw_string<LHS_N, CHECK_BOUNDS> const & lhs, fw_string<RHS_N, CHECK_BOUNDS> const & rhs) VR_NOEXCEPT
    {
        // TODO pcmpistri, handle different widths
        throw_x (illegal_state, "not implemented yet");
    }

    template<std::size_t _ = LHS_N>
    static VR_FORCEINLINE typename std::enable_if<(_ == RHS_N), bool>::type
    less (fw_string<LHS_N, CHECK_BOUNDS> const & lhs, fw_string<RHS_N, CHECK_BOUNDS> const & rhs) VR_NOEXCEPT
    {
        using signed_storage_type       = typename util::signed_type_of_size<LHS_N>::type;

        return fw_string_lexless<signed_storage_type>::evaluate (lhs.m_data, rhs.m_data);
    }

}; // end of class

// 'fw_string_heterogeneous_ops' impl:

template<std::size_t N, bool CHECK_BOUNDS>
VR_FORCEINLINE bool
fw_string_heterogeneous_ops<N, CHECK_BOUNDS>::equal (fw_string<N, CHECK_BOUNDS> const & lhs, string_literal_t const rhs) VR_NOEXCEPT
{
    storage_type rhs_data; // will be padded with zeros by the copy op below

    return (impl_ops_nochecks::copy_string_literal (rhs, rhs_data) & (lhs.m_data == rhs_data));
}

template<std::size_t N, bool CHECK_BOUNDS>
VR_FORCEINLINE bool
fw_string_heterogeneous_ops<N, CHECK_BOUNDS>::equal (fw_string<N, CHECK_BOUNDS> const & lhs, util::str_range const & rhs) VR_NOEXCEPT
{
    storage_type rhs_data; // will be padded with zeros by the copy op below

    return (impl_ops_nochecks::copy_string_range (rhs.m_start, rhs.m_size, rhs_data) & (lhs.m_data == rhs_data));
}

} // end of 'impl'
//............................................................................
//............................................................................

template<std::size_t LHS_N, std::size_t RHS_N, bool CHECK_BOUNDS>
VR_FORCEINLINE bool
operator== (fw_string<LHS_N, CHECK_BOUNDS> const & lhs, fw_string<RHS_N, CHECK_BOUNDS> const & rhs) VR_NOEXCEPT
{
    return impl::fw_string_ops<LHS_N, RHS_N, CHECK_BOUNDS>::equal (lhs, rhs);
}

template<std::size_t LHS_N, std::size_t RHS_N, bool CHECK_BOUNDS>
VR_FORCEINLINE bool
operator!= (fw_string<LHS_N, CHECK_BOUNDS> const & lhs, fw_string<RHS_N, CHECK_BOUNDS> const & rhs) VR_NOEXCEPT
{
    return (! (lhs == rhs));
}

template<std::size_t LHS_N, std::size_t RHS_N, bool CHECK_BOUNDS>
VR_FORCEINLINE bool
operator< (fw_string<LHS_N, CHECK_BOUNDS> const & lhs, fw_string<RHS_N, CHECK_BOUNDS> const & rhs) VR_NOEXCEPT
{
    return impl::fw_string_ops<LHS_N, RHS_N, CHECK_BOUNDS>::less (lhs, rhs);
}
//............................................................................
//............................................................................
namespace util
{

template<typename T>
struct is_fw_string: std::false_type
{
}; // end of master

template<std::size_t N, bool CHECK_BOUNDS>
struct is_fw_string<fw_string<N, CHECK_BOUNDS> >: std::true_type
{
}; // end of specialization

} // end of 'util'
//............................................................................
//............................................................................
} // end of namespace
//----------------------------------------------------------------------------
