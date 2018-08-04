#pragma once

#include "vr/asserts.h"
#include "vr/enums.h"
#include "vr/util/intrinsics.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
//............................................................................

// macro versions, for extremely premature optimization (avoid ABI overhead):
// [note: uses a gcc extension]

/**
 * @return [32 for zero input]
 */
#define vr_nlz_int32_t(x) \
    ({ int32_t r; asm ("lzcntl %1, %0" : "=r" (r) : "rm" (static_cast<uint32_t> (x))); r; })

/**
 * @return [64 for zero input]
 */
#define vr_nlz_int64_t(x) \
    ({ int64_t r; asm ("lzcntq %1, %0" : "=r" (r) : "rm" (static_cast<uint64_t> (x))); static_cast<int32_t> (r); })


//............................................................................

#define vr_rorx_int32(x, shift) \
    ({ int32_t r; asm ("rorxl %2, %1, %0" : "=r" (r) : "rm" (static_cast<uint32_t> (x)), "I" (shift)); r; })

#define vr_rorx_int64(x, shift) \
    ({ int64_t r; asm ("rorxq %2, %1, %0" : "=r" (r) : "rm" (static_cast<uint64_t> (x)), "J" (shift)); r; })

//............................................................................

VR_ENUM (zero_arg_policy,
    (
        ignore,         // caller knows zero input can't happen
        return_special  // return a caller-provided value
    )

); // end of enum

template<zero_arg_policy::enum_t ZERO_ARG_POLICY, int32_t ZERO_RETURN = -1>
struct arg_policy
{
    static const zero_arg_policy::enum_t zero_policy    = ZERO_ARG_POLICY;
    static const int32_t zero_return                    = ZERO_RETURN;

}; // end of trait

//............................................................................
//............................................................................
namespace impl
{

template<typename T, typename ZERO_ARG_POLICY, typename = void>
struct clz_impl
{
     static_assert (! std::is_integral<T>::value, "clz operation takes integer operands");
};

// NOTE: not using gcc builtins for this due to confusion about guarantees for zero inputs

template<typename T, typename ZERO_ARG_POLICY>
struct clz_impl<T, ZERO_ARG_POLICY,
    typename std::enable_if<(std::is_integral<T>::value/* && (sizeof (T) == sizeof (int32_t))*/)>::type>
{
    static int32_t evaluate (T x)
    {
        T r;

        if (zero_arg_policy::ignore == ZERO_ARG_POLICY::zero_policy) // compile-time branch
        {
            asm
            (
                "lzcnt %1, %0"
                : "=r" (r)
                : "rm" (x)
            );
        }
        else
        {

        }

        return r; // cast bast to 'int32_t'
    }

}; // end of specialization
//............................................................................

extern uint32_t const g_ilog10_i32_table_0 [11];
extern uint32_t const g_ilog10_i32_table_1 [11];

extern uint64_t const g_ilog10_i64_table_0 [20];
extern uint64_t const g_ilog10_i64_table_1 [20];

//............................................................................

template<typename T, typename ZERO_ARG_POLICY>
struct ilog10_floor_impl
{
    static int32_t evaluate (T x)
    {
        static_assert (lazy_false<T>::value, "ilog operation takes unsigned integer operands");
        VR_ASSUME_UNREACHABLE ();
    }

}; // end of master


template<typename ZERO_ARG_POLICY>
struct ilog10_floor_impl<uint32_t, ZERO_ARG_POLICY>
{
    static int32_t evaluate (uint32_t x)
    {
        int32_t y = (19 * (31 - vr_nlz_int32_t (x))) >> 6; // note: this relies on gcc doing arithmetic shift here (as it documents)

        switch (ZERO_ARG_POLICY::zero_return) // compile-time switch
        {
            case -1: return y + ((impl::g_ilog10_i32_table_1 [y + 1] - x) >> 31);
            case  0: return y + ((impl::g_ilog10_i32_table_0 [y + 1] - x) >> 31);
        }
    }

}; // end of specialization

template<typename ZERO_ARG_POLICY>
struct ilog10_floor_impl<uint64_t, ZERO_ARG_POLICY>
{
    static int32_t evaluate (uint64_t x)
    {
        int32_t y = (19 * (63 - vr_nlz_int64_t (x))) >> 6; // note: this relies on gcc doing arithmetic shift here (as it documents)

        switch (ZERO_ARG_POLICY::zero_return) // compile-time switch
        {
            case -1: return y + ((impl::g_ilog10_i64_table_1 [y + 1] - x) >> 63);
            case  0: return y + ((impl::g_ilog10_i64_table_0 [y + 1] - x) >> 63);
        }
    }

}; // end of specialization
//............................................................................

template<typename T, typename = void>
struct popcount_impl
{
    static int32_t evaluate (T)
    {
        static_assert (lazy_false<T>::value, "popcount operation takes integer operands");
        VR_ASSUME_UNREACHABLE ();
    }

}; // end of master


template<typename T>
struct popcount_impl<T,
    typename std::enable_if<(std::is_integral<T>::value && (sizeof (T) == sizeof (int32_t)))>::type>
{
    static VR_FORCEINLINE int32_t evaluate (T x)
    {
        return __builtin_popcount (x);
    }

}; // end of specialization

template<typename T>
struct popcount_impl<T,
    typename std::enable_if<(std::is_integral<T>::value && (sizeof (T) == sizeof (int64_t)))>::type>
{
    static VR_FORCEINLINE int32_t evaluate (T x)
    {
        return __builtin_popcountl (x);
    }

}; // end of specialization
//............................................................................

template<typename T, typename = void>
struct ls_zero_index_impl
{
    static int32_t evaluate (T)
    {
        static_assert (lazy_false<T>::value, "ls_zero_index operation takes integer operands");
        VR_ASSUME_UNREACHABLE ();
    }

}; // end of master


template<typename T>
struct ls_zero_index_impl<T,
    typename std::enable_if<(std::is_integral<T>::value && (sizeof (T) == sizeof (int32_t)))>::type>
{
    static VR_FORCEINLINE int32_t evaluate (T const x)
    {
        xmmi_t temp_0, temp_1;
        int32_t r;
        asm
        (
            "pxor %1, %1\n"
            "movd %3, %2\n"
            "pcmpistri %4, %1, %2\n"
            "movl %%ecx, %0\n"
                : "=&r" (r), "=&x" (temp_0), "=&x" (temp_1)
                : "rm" (x), "N" (_SIDD_CMP_EQUAL_EACH)
                : "%ecx", "cc"
        );

        return r;
    }

}; // end of specialization

template<typename T>
struct ls_zero_index_impl<T,
    typename std::enable_if<(std::is_integral<T>::value && (sizeof (T) == sizeof (int64_t)))>::type>
{
    static VR_FORCEINLINE int32_t evaluate (T const x)
    {
        xmmi_t temp_0, temp_1;
        int32_t r;
        asm
        (
            "pxor %1, %1\n"
            "movq %3, %2\n"
            "pcmpistri %4, %1, %2\n"
            "movl %%ecx, %0\n"
                : "=&r" (r), "=&x" (temp_0), "=&x" (temp_1)
                : "rm" (x), "N" (_SIDD_CMP_EQUAL_EACH)
                : "%ecx", "cc"
        );

        return r;
    }

}; // end of specialization

} // end of 'impl'
//............................................................................
//............................................................................

// TODO log2_ceil, log2_floor
// TODO log10_ceil, log10_floor

template<typename ZERO_ARG_POLICY, bool CHECK_INPUT = VR_CHECK_INPUT>
struct ops_int
{
    using zero_arg_policy           = ZERO_ARG_POLICY;


    template<typename T>
    static int32_t
    log2_floor (T x)
    {
        if (CHECK_INPUT) check_nonnegative (x);

        return ((sizeof (T) * 8 - 1) - impl::clz_impl<T, ZERO_ARG_POLICY>::evaluate (x));
    }

    template<typename T>
    static int32_t
    log2_ceil (T x)
    {
        return (1 + log2_floor (x - 1));
    }

    /**
     * TODO ZERO_ARG_POLICY
     *
     * @return [-1 for zero 'x']
     *
     * @ref H. Warren, 1st, p. 221
     */
    template<typename T>
    static int32_t
    ilog10_floor (T x)
    {
        if (CHECK_INPUT) check_nonnegative (x);

        using T_unsigned        = typename std::make_unsigned<T>::type;

        return impl::ilog10_floor_impl<T_unsigned, ZERO_ARG_POLICY>::evaluate (x);
    }

}; // end of scope
//............................................................................

template<typename T>
VR_FORCEINLINE int32_t
popcount (T x)
{
    return impl::popcount_impl<T>::evaluate (x);
}
//............................................................................
/**
 * @param x
 * @return index of the first null byte of 'x' assuming LE ('sizeof(T)' if none found)
 */
template<typename T>
VR_FORCEINLINE int32_t
ls_zero_index (T const x)
{
    return impl::ls_zero_index_impl<T>::evaluate (x);
}
//............................................................................
//............................................................................
namespace impl
{

template<typename ... Is>
struct mux_impl; // master

template<>
struct mux_impl<>
{
    template<typename _ = void>
    static VR_FORCEINLINE bitset32_t evaluate ()
    {
        static_assert (lazy_false<_>::value, "mux() requires at least one argument");
        VR_ASSUME_UNREACHABLE ();
    }

}; // end of specialization

template<typename I>
struct mux_impl<I>
{
    static VR_FORCEINLINE bitset32_t evaluate (I && v)
    {
        assert_within (v, 256);

        return static_cast<bitset32_t> (static_cast<uint8_t> (v));
    }

}; // end of specialization

template<typename I, typename ... Is>
struct mux_impl<I, Is ...>
{
    vr_static_assert (sizeof ... (Is) < sizeof (bitset32_t));

    static VR_FORCEINLINE bitset32_t evaluate (I && v, Is && ... values)
    {
        assert_within (v, 256);

        return (static_cast<bitset32_t> (static_cast<uint8_t> (v)) << (8 * sizeof ... (Is))) | mux_impl<Is ...>::evaluate (std::forward<Is> (values) ...);
    }

}; // end of specialization

} // end of 'impl'
//............................................................................
//............................................................................

template<typename ... Is>
VR_FORCEINLINE bitset32_t
mux (Is && ... values)
{
    return impl::mux_impl<Is ...>::evaluate (std::forward<Is> (values) ...);
}
//............................................................................

template<typename I>
I
byteswap (I const x); // master

// specializations:

template<>
VR_FORCEINLINE uint16_t
byteswap (uint16_t const x)
{
    return __builtin_bswap16 (x);
}

template<>
VR_FORCEINLINE uint32_t
byteswap (uint32_t const x)
{
    return __builtin_bswap32 (x);
}

template<>
VR_FORCEINLINE uint64_t
byteswap (uint64_t const x)
{
    return __builtin_bswap64 (x);
}
//............................................................................

template<typename I>
VR_FORCEINLINE I
net_to_host (I const x) // assumes 'host' is x86, little-endian
{
    return byteswap (x);
}
//............................................................................

template<typename I, bool LE = false>
struct net_field // specialized for big-endian 'field'
{
    static VR_FORCEINLINE I read (I const & field)
    {
        return byteswap (field);
    }

    static VR_FORCEINLINE void write (I & field, I const x)
    {
        field = byteswap (x);
    }

}; // end of specialization

template<typename I>
struct net_field<I, /* LE */true> // specialized for little-endian 'field'
{
    static VR_FORCEINLINE I const & read (I const & field) // NOTE: don't use with a literal param
    {
        return field; // pass through
    }

    static VR_FORCEINLINE void write (I & field, I const x)
    {
        field = x;
    }

}; // end of specialization
//............................................................................
//............................................................................
namespace impl
{

template<typename UI>
class net_int_base
{
    protected: // ............................................................

        vr_static_assert (std::is_unsigned<UI>::value);

        UI m_net_value;

}; // end of class

} // end of 'impl'
//............................................................................
//............................................................................

template<typename I, bool LE>
class net_int: public impl::net_int_base<typename std::make_unsigned<I>::type>
{
    private: // ..............................................................

        using UI            = typename std::make_unsigned<I>::type;
        using super         = impl::net_int_base<UI>;

        using this_type     = net_int<I, LE>;

        using rw_ops        = net_field<UI, LE>;

    public: // ...............................................................

        using value_type    = I;

        net_int (this_type const & rhs)   = default;
        net_int (this_type && rhs)              = default;

        /**
         * @note leaves content uninitialized!
         */
        VR_FORCEINLINE net_int ()
        {
        }

        VR_FORCEINLINE explicit net_int (I const & x)
        {
            rw_ops::write (super::m_net_value, x);
        }

        // read:

        VR_FORCEINLINE operator I () const
        {
            return rw_ops::read (super::m_net_value);
        }

        // write:

        this_type & operator= (this_type const & rhs)   = default;
        this_type & operator= (this_type && rhs)        = default;

        VR_FORCEINLINE void operator= (I const & rhs)
        {
            rw_ops::write (super::m_net_value, rhs);
        }

}; // end of class
//............................................................................
//............................................................................
namespace impl
{

template<typename T, typename = void>
struct integral_cast_impl; // master


template<typename T>
struct integral_cast_impl<T,
    typename std::enable_if<std::is_integral<T>::value>::type>
{
    using result_type       = typename util::unsigned_type_of_size<sizeof (T)>::type; // unsigned

    static VR_FORCEINLINE result_type evaluate (T const v)
    {
        return static_cast<result_type> (v);
    }

}; // end of specialization

template<typename T>
struct integral_cast_impl<T,
    typename std::enable_if<std::is_pointer<T>::value>::type>
{
    using result_type       = std_uintptr_t; // unsigned

    static VR_FORCEINLINE result_type evaluate (T const v)
    {
        return reinterpret_cast<result_type> (v);
    }

}; // end of specialization
//............................................................................

template<typename T, typename = void>
struct bit_cast_impl; // master


template<typename T>
struct bit_cast_impl<T,
    typename std::enable_if<util::is_integral_or_pointer<T>::value>::type>
{
    using result_type       = typename unsigned_type_of_size<sizeof (T)>::type;

    static VR_FORCEINLINE result_type evaluate (T const & x)
    {
        return static_cast<result_type> (x);
    }

}; // end of specialization

template<typename T>
struct bit_cast_impl<T,
    typename std::enable_if<std::is_floating_point<T>::value>::type>
{
    using result_type       = typename unsigned_type_of_size<sizeof (T)>::type;

    union fp_alias
    {
        T m_value;
        result_type m_image;

    }; // end of local union

    static VR_FORCEINLINE result_type evaluate (T const & x)
    {
        return reinterpret_cast<fp_alias const *> (& x)->m_image;
    }

}; // end of specialization

} // end of 'impl'
//............................................................................
//............................................................................

template<typename T>
typename unsigned_type_of_size<sizeof (T)>::type
integral_cast (T const & x)
{
    return impl::integral_cast_impl<T>::evaluate (x);
}

template<typename T>
typename unsigned_type_of_size<sizeof (T)>::type
bit_cast (T const & x)
{
    return impl::bit_cast_impl<T>::evaluate (x);
}

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
