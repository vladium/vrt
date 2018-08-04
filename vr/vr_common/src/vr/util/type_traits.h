#pragma once

#include "vr/macros.h"
#include "vr/types.h"

#include <array>
#include <iterator>
#include <tuple>
#include <type_traits>
#include <utility> // std::declval

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
//............................................................................
//............................................................................
namespace impl
{

template<typename ... Ts>
struct make_void
{
    using type  = void;
};

} // end of 'impl'
//............................................................................
//............................................................................
/**
 * a simple(r) alternative to enable_if/disable_if [c++17], with a workaround
 * for the "bug" introduced by c++14
 *
 * @see http://stackoverflow.com/questions/27687389/how-does-void-t-work
 */
template<typename ... Ts>
using void_t                        = typename impl::make_void<Ts ...>::type;

//............................................................................
/**
 * [c++14]
 */
template<bool B, typename T = void>
using enable_if_t                   = typename std::enable_if<B, T>::type;

//............................................................................
/**
 * a less verbose, MPL-style, alias
 */
template<typename T, T V>
using int_                          = std::integral_constant<T, V>;


template<int32_t V>
using int32_                        = int_<int32_t, V>;

/**
 * [c++17]
 */
template<bool B>
using bool_constant                 = int_<bool, B>;

//............................................................................

template<bool B, typename T, typename F>
using if_c                          = std::conditional<B, T, F>;

template<bool B, typename T, typename F>
using if_t                          = typename if_c<B, T, F>::type;

//............................................................................

template<bool B, typename T>
using add_const_if_t                = if_t<B, typename std::add_const<T>::type, T>;

//............................................................................
//............................................................................
namespace impl
{

template<typename T, typename ... Us> // TODO could be re-implemented via 'index_of' (except for duplicate entries)
struct contains_impl; // forward


template<typename T>
struct contains_impl<T>: bool_constant<false>
{
}; // end of specialization

template<typename T, typename U, typename ... Us>
struct contains_impl<T, U, Us ...>: bool_constant<(std::is_same<T, U>::value || contains_impl<T, Us...>::value)>
{
}; // end of specialization
//............................................................................

template<int32_t I, typename T, typename ... Us> // TODO could be re-implemented via 'index_of' (except for duplicate entries)
struct index_of_impl; // forward


template<int32_t I, typename T>
struct index_of_impl<I, T>
{
    static constexpr int32_t value  = -1;

}; // end of specialization

template<int32_t I, typename T, typename U, typename ... Us>
struct index_of_impl<I, T, U, Us ...>
{
    static constexpr int32_t value  = (std::is_same<T, U>::value ? I : index_of_impl<(I + 1), T, Us...>::value);

}; // end of specialization
//............................................................................

template<typename BASE, typename ... Ts>
struct find_derived_impl; // forward

struct _not_found_;

template<typename BASE>
struct find_derived_impl<BASE>
{
    using type          = _not_found_;

}; // end of specialization

template<typename BASE, typename T, typename ... Ts>
struct find_derived_impl<BASE, T, Ts ...>
{
    using type         = if_t<(std::is_base_of<BASE, T>::value), T, typename find_derived_impl<BASE, Ts ...>::type>;

}; // end of specialization

} // end of 'impl'
//............................................................................
//............................................................................
/**
 * [~ c++17 std::disjunction]
 */
template<typename T, typename ... Ts>
struct contains: impl::contains_impl<T, Ts ...>
{
}; // end of metafunction

template<typename T, typename ... Ts>
struct index_of
{
    static constexpr int32_t value  = impl::index_of_impl<0, T, Ts ...>::value;

}; // end of metafunction
//............................................................................
/*
 * selects first ('T' is a subclass of 'BASE') match in 'Ts' (or 'T_DEFAULT' if no match)
 */
template<typename BASE, typename T_DEFAULT, typename ... Ts>
struct find_derived
{
    using match         = typename impl::find_derived_impl<BASE, Ts ...>::type;
    using type          = if_t<std::is_same<match, impl::_not_found_>::value, T_DEFAULT, match>;

}; // end of metafunction

template<typename BASE, typename T_DEFAULT, typename ... Ts>
using find_derived_t                = typename find_derived<BASE, T_DEFAULT, Ts ...>::type;

//............................................................................

template<typename T>
using lazy_false                    = if_t<std::is_same<T, T>::value, std::false_type, std::false_type>;

template<bool B>
using lazy_false_c                  = if_t<B, std::false_type, std::false_type>;

//............................................................................

template<typename T>
struct is_void: std::is_same<void, T>
{
}; // end of metafunction

template<typename T, typename T_TRUE, typename T_FALSE>
using if_is_void                    = if_c<is_void<T>::value, T_TRUE, T_FALSE>;

template<typename T, typename T_TRUE, typename T_FALSE>
using if_is_void_t                  = typename if_is_void<T, T_TRUE, T_FALSE>::type;

//............................................................................
/**
 * a CRTP helper: choose 'DERIVED' is it's not void, otherwise choose 'T'
 */
template<typename T, typename DERIVED>
using this_or_derived_t             = if_is_void_t<DERIVED, T, DERIVED>;

//............................................................................

template<typename T>
struct is_bool: std::is_same<T, bool>
{
}; // end of metafunction
//............................................................................

template<typename T>
struct is_char: std::is_same<T, char>
{
}; // end of metafunction
//............................................................................

template<typename T>
struct is_std_string: std::is_same<T, std::string>
{
}; // end of metafunction
//............................................................................

template<typename T>
struct is_pair: std::false_type
{
}; // end of master

template<typename T1, typename T2>
struct is_pair<std::pair<T1, T2> >: std::true_type
{
}; // end of specialization
//............................................................................
/*
 * c.f. std::is_array
 */
template<typename T>
struct is_std_array: std::false_type
{
}; // end of master

template<typename V, std::size_t N>
struct is_std_array<std::array<V, N> >: std::true_type
{
}; // end of specialization
//............................................................................

template<typename T>
struct is_tuple: std::false_type
{
}; // end of master

template<typename ... Ts>
struct is_tuple<std::tuple<Ts ...>>: std::true_type
{
}; // end of specialization
//............................................................................
/*
 * note: only support default values for all but first std::vector<> template params
 */
template<typename T>
struct is_std_vector: std::false_type
{
}; // end of master

template<typename V>
struct is_std_vector<std::vector<V> >: std::true_type
{
}; // end of specialization
//............................................................................

template<typename T>
struct is_std_char_array: std::false_type
{
}; // end of master

template<std::size_t N>
struct is_std_char_array<std::array<char, N> >: std::true_type
{
}; // end of specialization
//............................................................................

template<typename T, typename U>
struct enable_if_is_same: std::enable_if<std::is_same<T, U>::value>
{
}; // end of metafunction
//............................................................................

template<typename SRC, typename DST>
class copy_cv_reference
{
    using non_ref       = typename std::remove_reference<SRC>::type;

    using t0            = if_t<std::is_const<non_ref>::value, typename std::add_const<DST>::type, DST>;
    using t1            = if_t<std::is_volatile<non_ref>::value, typename std::add_volatile<t0>::type, t0>;

    using t2            = if_t<std::is_lvalue_reference<SRC>::value, typename std::add_lvalue_reference<t1>::type, t1>;
    using t3            = if_t<std::is_rvalue_reference<SRC>::value, typename std::add_rvalue_reference<t2>::type, t2>;

    public:

        using type      = t3;

}; // end of metafunction

template<typename SRC, typename DST>
using copy_cv_reference_t               = typename copy_cv_reference<SRC, DST>::type;

//............................................................................
#if (BOOST_GCC_VERSION < 50000)

    template<typename T, typename ... ARGs>
    using is_trivially_constructible        = std::has_trivial_default_constructor<T>;

#else // later c++11 than available in gcc 4.x:

    template<typename T, typename ... ARGs>
    using is_trivially_constructible        = std::is_trivially_constructible<T, ARGs ...>;

#endif // (BOOST_GCC_VERSION < 50000)

template<typename T, typename ... ARGs>
using is_trivially_constructible_t      = typename is_trivially_constructible<T, ARGs ...>::type;

//............................................................................
#if (BOOST_GCC_VERSION < 50000)

    template<typename T>
    using is_trivially_copy_assignable      = std::has_trivial_copy_assign<T>;

#else // later c++11 than available in gcc 4.x:

    template<typename T>
    using is_trivially_copy_assignable      = std::is_trivially_copy_assignable<T>;

#endif // (BOOST_GCC_VERSION < 50000)

template<typename T>
using is_trivially_copy_assignable_t    = typename is_trivially_copy_assignable<T>::type;

//............................................................................

template<typename> struct array_size; // master

/*
 * a workaround for std::array::[max_]size() not acceptable in constexpr's
 *
 * @see c_array_type in "types.h"
 *
 * TODO rename 'array_extent'
 * TODO merge this with (renamed) 'length()' in "types.h"
 */
template<typename T, std::size_t N>
struct array_size<std::array<T, N> >
{
    static constexpr std::size_t value  = N;

}; // end of specialization
//............................................................................

// TODO <boost/integer.hpp> also has this

template<std::size_t SIZEOF>
struct signed_type_of_size; // master

template<>
struct signed_type_of_size<1>
{
    using type                      = int8_t;

}; // end of specialization

template<>
struct signed_type_of_size<2>
{
    using type                      = int16_t;

}; // end of specialization

template<>
struct signed_type_of_size<4>
{
    using type                      = int32_t;

}; // end of specialization

template<>
struct signed_type_of_size<8>
{
    using type                      = int64_t;

}; // end of specialization


template<std::size_t SIZEOF>
struct unsigned_type_of_size
{
    using type                      = typename std::make_unsigned<typename signed_type_of_size<SIZEOF>::type>::type;

}; // end of metafunction
//............................................................................

template<bool SIGNED, std::size_t SIZEOF>
struct int_type_of_size
{
    using type                      = typename if_c<SIGNED, typename signed_type_of_size<SIZEOF>::type, typename unsigned_type_of_size<SIZEOF>::type>::type;
};


template<bool SIGNED, std::size_t SIZEOF>
using int_type_of_size_t            = typename int_type_of_size<SIGNED, SIZEOF>::type;

//............................................................................

template<std::size_t SIZEOF>
struct fp_type_of_size; // master


template<>
struct fp_type_of_size<sizeof (float)>
{
    using type                      = float;

}; // end of specialization

template<>
struct fp_type_of_size<sizeof (double)>
{
    using type                      = double;

}; // end of specialization
//............................................................................

template<typename T>
struct is_integral_or_pointer: bool_constant<(std::is_integral<T>::value || std::is_pointer<T>::value)>
{
}; // end of metafunction

template<typename T>
struct is_fundamental_or_pointer: bool_constant<(std::is_fundamental<T>::value || std::is_pointer<T>::value)>
{
}; // end of metafunction
//............................................................................

template<typename T, typename OS, typename = void>
struct is_streamable: std::false_type
{
}; // end of master

template<typename T, typename OS>
struct is_streamable<T, OS, void_t<decltype ((std::declval<OS &> () << std::declval<T> ()))> >: std::true_type
{
}; // end of specialization
//............................................................................

template<typename T, typename = void>
struct has_print: std::false_type
{
}; // end of master

template<typename T>
struct has_print<T, void_t<decltype (__print__ (std::declval<T> ()))> >: std::true_type
{
}; // end of specialization
//............................................................................

template<typename C, typename = void>
struct has_empty: std::false_type
{
}; // end of master

template<typename C>
struct has_empty<C, void_t<decltype (std::declval<C> ().empty ())> >: std::true_type
{
}; // end of specialization
//............................................................................
namespace impl
{

using std::begin;   // allow ADL
using std::end;     // allow ADL

template<typename C, typename = void>
struct is_iterable_impl: std::false_type
{
}; // end of master

template<typename C>
struct is_iterable_impl<C, void_t<decltype (std::declval<C> ().begin ()), decltype (std::declval<C> ().end ())> >: std::true_type
{
}; // end of specialization

} // end of 'impl'
//............................................................................
//............................................................................

template<typename C>
struct is_iterable: impl::is_iterable_impl<C>
{
}; // end of metafunction

} // end of 'util'
//............................................................................
//............................................................................
/*
 * TODO handle rvalues better, don't force a ref arg
 */
template<typename T>
constexpr VR_FORCEINLINE typename std::make_unsigned<T>::type const &
unsigned_cast (const T & x)
{
    using result_type       = typename std::make_unsigned<T>::type;
    using cv_void           = util::copy_cv_reference_t<T, void>;

    return (* static_cast<result_type const *> (static_cast<cv_void const *> (& x)));
}

template<typename T>
VR_FORCEINLINE typename std::make_unsigned<T>::type &
unsigned_cast (T & x)
{
    using result_type       = typename std::make_unsigned<T>::type;
    using cv_void           = util::copy_cv_reference_t<T, void>;

    return (* static_cast<result_type *> (static_cast<cv_void *> (& x)));
}

template<typename T>
constexpr VR_FORCEINLINE typename std::make_signed<T>::type const &
signed_cast (const T & x)
{
    using result_type       = typename std::make_signed<T>::type;
    using cv_void           = util::copy_cv_reference_t<T, void>;

    return (* static_cast<result_type const *> (static_cast<cv_void const *> (& x)));
}

template<typename T>
VR_FORCEINLINE typename std::make_signed<T>::type &
signed_cast (T & x)
{
    using result_type       = typename std::make_signed<T>::type;
    using cv_void           = util::copy_cv_reference_t<T, void>;

    return (* static_cast<result_type *> (static_cast<cv_void *> (& x)));
}
//............................................................................

// helpers for "type-safety" inside macros:

template<typename T>
VR_FORCEINLINE std_intptr_t
intptr (T * const p)
{
    return reinterpret_cast<std_intptr_t> (p);
}

template<typename T>
VR_FORCEINLINE std_uintptr_t
uintptr (T * const p)
{
    return reinterpret_cast<std_uintptr_t> (p);
}
//............................................................................

template<typename T>
VR_FORCEINLINE uint8_t const *
byte_ptr_cast (T const * const p)
{
    return reinterpret_cast<uint8_t const *> (p);
}

template<typename T>
VR_FORCEINLINE uint8_t *
byte_ptr_cast (T * const p)
{
    return reinterpret_cast<uint8_t *> (p);
}
//............................................................................

template<typename T>
VR_FORCEINLINE char const *
char_ptr_cast (T const * const p)
{
    return reinterpret_cast<char const *> (p);
}

template<typename T>
VR_FORCEINLINE char *
char_ptr_cast (T * const p)
{
    return reinterpret_cast<char *> (p);
}

template<typename T, std::size_t N>
VR_FORCEINLINE typename c_array_type<char, N>::const_type &
char_array_cast (T const (& a) [N])
{
    return reinterpret_cast<typename c_array_type<char, N>::const_type> (a);
}

template<typename T, std::size_t N>
VR_FORCEINLINE typename c_array_type<char, N>::type &
char_array_cast (T (& a) [N])
{
    return reinterpret_cast<typename c_array_type<char, N>::type> (a);
}
//............................................................................

template<typename I>
VR_FORCEINLINE addr_const_t
addr_plus (addr_const_t const a, I const offset)
{
    return (static_cast<int8_t const *> (a) + offset);
}

template<typename I>
VR_FORCEINLINE addr_t
addr_plus (addr_t const a, I const offset)
{
    return (static_cast<int8_t *> (a) + offset);
}

} // end of namespace
//----------------------------------------------------------------------------
