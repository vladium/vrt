#pragma once

#include "vr/enums.h"
#include "vr/meta/integer.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace data
{
//............................................................................

using width_type                    = int32_t;

//............................................................................

// TODO fixed-width strings

#define VR_ATYPE_SEQ    \
            (category)  \
            (timestamp) \
            (i8)        \
            (f8)        \
            (i4)        \
            (f4)        \
    /* */

#define VR_ATYPE_TYPE_SEQ   \
            (enum_int_t)    \
            (timestamp_t)   \
            (int64_t)       \
            (double)        \
            (int32_t)       \
            (float)         \
    /* */
//............................................................................

struct atype final
{
    VR_NESTED_ENUM (
        (
            BOOST_PP_SEQ_ENUM (VR_ATYPE_SEQ)
        ),
        iterable, printable, parsable

    ) // end of enum

    // TRAITs:

    /**
     * lookup data storage type for 'ATYPE'
     */
    template<enum_t ATYPE> // specialized below
    struct traits; // end of nested class

    /**
     * lookup in the inverse direction to that of 'traits'
     */
    template<typename T>
    static constexpr enum_t for_numeric ()  { return for_numeric_impl<T>::value; }


    static constexpr int32_t const width [size] =
    {
#   define vr_SIZE(r, data, i, ATYPE)   BOOST_PP_COMMA_IF (i) sizeof (ATYPE)

         BOOST_PP_SEQ_FOR_EACH_I (vr_SIZE, unused, VR_ATYPE_TYPE_SEQ)

#   undef vr_SIZE
    };


    private:

        template<typename T, typename = void> // specialized/enabled below
        struct for_numeric_impl; // end of nested class

}; // end of class
//............................................................................

vr_static_assert (! atype::category); // exploited by 'attr_type'
static_assert (atype::size <= 8, "too many values to keep in low pointer bits");

//............................................................................
#define vr_TRAIT_SP(r, data, i, ATYPE) \
    template<> \
    struct atype::traits<atype:: ATYPE > \
    { \
        using type                      = BOOST_PP_SEQ_ELEM (i, VR_ATYPE_TYPE_SEQ); \
        static constexpr int32_t width  = sizeof (type); \
    }; \
    /* */

    BOOST_PP_SEQ_FOR_EACH_I (vr_TRAIT_SP, unused, VR_ATYPE_SEQ)


#undef vr_TRAIT_SP
//............................................................................

template<typename T>
struct atype::for_numeric_impl<T,
    typename std::enable_if<std::is_integral<T>::value>::type>
{
    static constexpr enum_t value       = (sizeof (T) == traits<i4>::width ? i4 : i8);

}; // end of specialization

template<typename T>
struct atype::for_numeric_impl<T,
    typename std::enable_if<std::is_floating_point<T>::value>::type>
{
    static constexpr enum_t value       = (sizeof (T) == traits<f4>::width ? f4 : f8);

}; // end of specialization
//............................................................................
//............................................................................

// dispatch assists:

template<atype::enum_t ATYPE>
using atype_                        = util::int_<atype::enum_t, ATYPE>;

//............................................................................
/**
 * a static_cast from an atype::enum_t 'at' to 'T' is possible if 'castable<T>::atypes' contains 'at'
 */
template<typename T, typename = void>
struct castable
{
    static_assert (util::lazy_false<T>::value, "'T' not supported for mapping to 'atype'");

}; // end of master


template<typename T>
struct castable<T,
    typename std::enable_if<(std::is_integral<T>::value && (sizeof (T) == 8))>::type>
{
    static constexpr bitset32_t atypes  = meta::static_bitset<bitset32_t, atype::timestamp, atype::i8>::value;

}; // end of specialization

template<typename T>
struct castable<T,
    typename std::enable_if<(std::is_integral<T>::value && (sizeof (T) == 4))>::type>
{
    static constexpr bitset32_t atypes  = meta::static_bitset<bitset32_t, atype::category, atype::i4>::value;

}; // end of specialization

template<>
struct castable<double, void>
{
    static constexpr bitset32_t atypes  = meta::static_bitset<bitset32_t, atype::f8>::value;

}; // end of specialization

template<>
struct castable<float, void>
{
    static constexpr bitset32_t atypes  = meta::static_bitset<bitset32_t, atype::f4>::value;

}; // end of specialization


template<typename E> // specialize for typesafe enums
struct castable<E,
    typename std::enable_if<is_typesafe_enum<E>::value>::type>
{
    static constexpr bitset32_t atypes  = meta::static_bitset<bitset32_t, atype::category, atype::i4>::value;

}; // end of specialization

template<typename T> // specialize for plain enums
struct castable<T,
    typename std::enable_if<std::is_enum<T>::value>::type>
{
    static constexpr bitset32_t atypes  = meta::static_bitset<bitset32_t, atype::category, atype::i4>::value;

}; // end of specialization
//............................................................................

template<typename V, typename ... ARGs>
VR_FORCEINLINE auto
dispatch_on_atype (atype::enum_t const e, V && visitor, ARGs && ... args)
    -> decltype (visitor (data::atype_<data::atype::first> { }, std::forward<ARGs> (args) ...)) // infer return type from 'atype::first' overload, if any, only
{
    switch (e)
    {
#   define vr_VISIT(r, unused, e_name) \
        case data::atype:: e_name : return visitor (data::atype_<data::atype:: e_name > { }, std::forward<ARGs> (args) ...); \
        /* */

        BOOST_PP_SEQ_FOR_EACH (vr_VISIT, unused, VR_ATYPE_SEQ);

#   undef vr_VISIT

        default: VR_ASSUME_UNREACHABLE ();

    } // end of switch
}

} // end of 'data'
} // end of namespace
//----------------------------------------------------------------------------
