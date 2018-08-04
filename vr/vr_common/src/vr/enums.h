#pragma once

#include "vr/impl/enums_impl.h"
#include "vr/util/type_traits.h"

#include <boost/preprocessor/seq/enum.hpp> // not used by enum impl but is a common need when defining them

//----------------------------------------------------------------------------
namespace vr
{
//............................................................................
/**
 * A macro invocation like this:
 *@code
 *      VR_ENUM (SOME_ENUM,
 *          (A, B, C),
 *          iterable, printable, parsable
 *      );
 *@endcode
 * generates a 'typesafe' enum of the following structure:
 *@code
 *      struct SOME_ENUM final
 *      {
 *          enum enum_t : enum_int_t
 *          {
 *              A, first = A,
 *              B,
 *              C,
 *
 *              size,
 *              na
 *          };
 *          [optional operator overloads for 'SOME_ENUM::enum_t']
 *
 *      }; // end of enum
 *@endcode
 *  where the (variadic) list of options enables the following features:
 *
 *  - 'iterable' : operator++ for classic for-loops as well as for new for(:)-loop syntax
 *  - 'printable': operator<<, name(e), __print__
 *  - 'parsable' : value(str)
 */
#define VR_ENUM(name, vals, /* iterable, printable, parsable */...) \
    vr_ENUM_PROLOGUE (name)                                         \
    VR_NESTED_ENUM (vals, __VA_ARGS__)                              \
    vr_ENUM_EPILOGUE ()                                             \
    /* */
//............................................................................
/**
 * Similar to @ref VR_ENUM() but allows custom user definition of the wrapping
 * struct:
 *@code
 *      struct SOME_ENUM
 *      {
 *          VR_NESTED_ENUM ((A, B, C), iterable);
 *          .. additional methods ...
 *
 *      }; // end of class
 *@endcode
 */
#define VR_NESTED_ENUM(vals, /* iterable, printable, parsable */...)    \
    vr_ENUM_VALUES (BOOST_PP_TUPLE_TO_SEQ (vals))                       \
    vr_ENUM_TRAITS (__VA_ARGS__)                                        \
    vr_ENUM_OPTIONS (BOOST_PP_TUPLE_TO_SEQ (vals), __VA_ARGS__)         \
    /* */

//............................................................................
/**
 * A macro invocation like this:
 *@code
 *      VR_EXPLICIT_ENUM (SOME_ENUM, char,
 *          ('a',   A)
 *          ('b',   B)
 *          ('c',   C),
 *          printable, parsable
 *      );
 *@endcode
 * generates a 'typesafe' enum of the following structure:
 *@code
 *      struct SOME_ENUM final
 *      {
 *          enum enum_t : char
 *          {
 *              A   = 'a',
 *              B   = 'b',
 *              C   = 'c
 *          };
 *          [optional operator overloads for 'SOME_ENUM::enum_t']
 *
 *      }; // end of enum
 *@endcode
 *  where the (variadic) list of options enables the following features:
 *
 *  - 'printable': operator<<, name(e), __print__
 *  - 'parsable' : value(str)
 */
#define VR_EXPLICIT_ENUM(name, type, vals, /* printable, parsable */...)    \
    vr_ENUM_PROLOGUE (name)                                                 \
    VR_NESTED_EXPLICIT_ENUM (type, vals, __VA_ARGS__)                       \
    vr_ENUM_EPILOGUE ()                                                     \
    /* */

#define VR_NESTED_EXPLICIT_ENUM(type, vals, /* printable, parsable */...)   \
    vr_EXPLICIT_ENUM_VALUES (type, VR_DOUBLE_PARENTHESIZE_2 (vals))         \
    vr_ENUM_TRAITS (__VA_ARGS__)                                            \
    vr_EXPLICIT_ENUM_OPTIONS (VR_DOUBLE_PARENTHESIZE_2 (vals), __VA_ARGS__) \
    /* */

//............................................................................

template<typename E, typename = void>
struct is_typesafe_enum: std::false_type
{
}; // end of master

template<typename E>
struct is_typesafe_enum<E,
    typename std::enable_if<std::is_enum<typename E::enum_t>::value>::type>: std::true_type
{
}; // end of specialization


template<typename E>
using is_typesafe_enum_t            = typename is_typesafe_enum<E>::type;

//............................................................................

template<typename E>
struct is_enum_iterable: public util::bool_constant<std::is_base_of<_iterable_, typename E::enum_traits>::value> { };

template<typename E>
struct is_enum_printable: public util::bool_constant<std::is_base_of<_printable_, typename E::enum_traits>::value> { };

template<typename E>
struct is_enum_parsable: public util::bool_constant<std::is_base_of<_parsable_, typename E::enum_traits>::value> { };

//............................................................................

template<typename T, typename = void>
struct lower_if_typesafe_enum
{
    using type                      = T;

}; // end of master

template<typename T>
struct lower_if_typesafe_enum<T,
    typename std::enable_if<is_typesafe_enum<T>::value>::type>
{
    using type                      = typename T::enum_t;

}; // end of specialization


template<typename T>
using lower_if_typesafe_enum_t      = typename lower_if_typesafe_enum<T>::type;

//............................................................................
/**
 * convenience analogue of 'util::int_' for typesafe enums
 */
template<typename E, typename E::enum_t V>
using enum_                         = util::int_<typename E::enum_t, V>;

//............................................................................

template<typename E> // enabled to provide a more helpful compiler error
typename std::enable_if<(! is_typesafe_enum_t<E>::value)>::type
to_enum (string_literal_t const name)
{
    static_assert (is_typesafe_enum_t<E>::value, "'E' is not a typesafe enum type");
}

template<typename E> // enabled to provide a more helpful compiler error
typename std::enable_if<(! is_typesafe_enum_t<E>::value)>::type
to_enum (std::string const & name)
{
    static_assert (is_typesafe_enum_t<E>::value, "'E' is not a typesafe enum type");
}


template<typename E>
typename std::enable_if<is_typesafe_enum_t<E>::value, typename E::enum_t>::type
to_enum (char_const_ptr_t const name, int32_t const name_size)
{
    static_assert (is_enum_parsable<E>::value, "'E' is not parsable");

    return E::value (name, name_size);
}

template<typename E>
typename std::enable_if<is_typesafe_enum_t<E>::value, typename E::enum_t>::type
to_enum (string_literal_t const name)
{
    static_assert (is_enum_parsable<E>::value, "'E' is not parsable");

    return E::value (name);
}

template<typename E>
typename std::enable_if<is_typesafe_enum_t<E>::value, typename E::enum_t>::type
to_enum (std::string const & name)
{
    static_assert (is_enum_parsable<E>::value, "'E' is not parsable");

    return E::value (name);
}
//............................................................................

} // end of namespace
//----------------------------------------------------------------------------
