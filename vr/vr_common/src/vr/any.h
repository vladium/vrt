#pragma once

#include "vr/enums.h"
#include "vr/exceptions.h"
#include "vr/util/numeric.h"

#include <boost/any.hpp> // std in c++17

#include <functional> // std::[c]ref
#include <utility>

//----------------------------------------------------------------------------
namespace vr
{
//............................................................................

using any                   = boost::any;

//............................................................................
//............................................................................
namespace impl
{

template<typename T, typename = void>
struct any_get_impl
{
    using T_no_ref      = typename std::remove_reference<T>::type;
    using T_bare        = typename std::remove_const<T_no_ref>::type;

    using T_cref_w      = std::reference_wrapper<const T_bare>;

    static T_bare const & evaluate (any const & v) // return by const ref
    {
        T_bare const * const vp = boost::any_cast<T_bare const> (& v);
        if (VR_LIKELY (vp != nullptr))
            return (* vp);

        T_cref_w const * const cwp = boost::any_cast<T_cref_w> (& v);
        if (cwp != nullptr)
            return static_cast<T_bare const &> (* cwp);

        throw_x (type_mismatch, "attempt to cast value of type {" + util::class_name (v.type ()) + "} to {" + util::class_name<T> () + '}');
    }

}; // end of master


template<typename T>
struct any_get_impl<T, // specialize for integer types
    util::enable_if_t<std::is_integral<std::remove_cv_t<T>>::value>>
{
    using T_bare        = std::remove_cv_t<T>;

    static T_bare evaluate (any const & v) // return by value
    {
        // first try the requested type:
        {
            T_bare const * const vp = boost::any_cast<T_bare> (& v);
            if (vp) return ( * vp);
        }
        // next try a numeric cast:
        {
            std::type_info const & v_type = v.type ();

#       define vr_TYPE(T) \
            if (v_type == typeid ( T )) return util::numeric_cast<T_bare> (boost::any_cast< T > (v)); \
            /* */

            vr_TYPE  (int64_t)
            vr_TYPE (uint64_t)
            vr_TYPE  (int32_t)
            vr_TYPE (uint32_t)
            vr_TYPE  (int16_t)
            vr_TYPE (uint16_t)

#       undef vr_TYPE
        }

        throw_x (type_mismatch, "can't cast value of type {" + util::class_name (v.type ()) + "} to {" + util::class_name<T> () + '}');
    }

}; // end of specialization

template<typename T>
struct any_get_impl<T, // specialize for std::strings and C-strings
    util::enable_if_t<std::is_same<std::string, std::remove_cv_t<T>>::value>>
{
    static std::string evaluate (any const & v) // return by value
    {
        {
            string_literal_t const * const s = boost::any_cast<string_literal_t> (& v);
            if (s) return { * s };
        }
        {
            std::string const * const s = boost::any_cast<std::string> (& v);
            if (s) return (* s);
        }

        throw_x (type_mismatch, "can't cast value of type {" + util::class_name (v.type ()) + "} to a string");
    }

}; // end of specialization

template<typename E>
struct any_get_impl<E, // specialize for typesafe enums
    util::enable_if_t<is_typesafe_enum<E>::value>>
{
    static typename E::enum_t evaluate (any const & v) // return enum value
    {
        // try the numeric enum value:
        {
            typename E::enum_t const * const e = boost::any_cast<typename E::enum_t> (& v);
            if (e) return (* e);
        }
        // try the value name:
        try
        {
            std::string const s { any_get_impl<std::string>::evaluate (v) };
            return to_enum<E> (s); // TODO check 'E' for 'parsable' option/trait
        }
        catch (type_mismatch const &) { }

        throw_x (type_mismatch, "can't cast value of type {" + util::class_name (v.type ()) + "} to {" + util::class_name<E> () + '}');
    }

}; // end of specialization

} // end of 'impl'
//............................................................................
//............................................................................

// TODO functions renamed to avoid overload ambiguities or having to use top-level namespace

/**
 * @note const object references can be passed via 'std::cref()'
 */
template<typename T>
auto
any_get (any const & v) -> decltype (impl::any_get_impl<T>::evaluate (v))
{
    return impl::any_get_impl<T>::evaluate (v);
}
//............................................................................
/*
 * non-throwing pointer cast overloads:
 */
template<typename T>
const T *
any_get (any const * const p) VR_NOEXCEPT
{
    return boost::any_cast<T> (p);
}

template<typename T>
T *
any_get (any * const p) VR_NOEXCEPT
{
    return boost::any_cast<T> (p);
}

} // end of namespace
//----------------------------------------------------------------------------
