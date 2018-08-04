#pragma once

#include "vr/strings.h"
#include "vr/util/type_traits.h"

#include <typeindex>
#include <typeinfo>

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
//............................................................................

#define VR_CN(cls)  util::class_name< cls > ()

//............................................................................
//............................................................................
namespace impl
{

template<typename T, typename = void>
struct default_construct_impl
{
    static bool evaluate (T & obj)
    {
        return false;
    }

}; // end of master

template<typename T>
struct default_construct_impl<T, // specialize to trap incorrect usage
    typename std::enable_if<std::is_pointer<T>::value>::type>
{
    static VR_NORETURN bool evaluate (T & obj)
    {
        static_assert (lazy_false<T>::value, "util::default_construct_if_possible() invoked on a pointer");
    }

}; // end of specialization

template<typename T>
struct default_construct_impl<T,
    typename std::enable_if<std::is_default_constructible<T>::value>::type>
{
    static bool evaluate (T & obj)
    {
        new (& obj) T { };

        return true;
    }

}; // end of specialization
//............................................................................

template<typename T, typename = void>
struct destruct_impl
{
    static void evaluate (T & obj)
    {
        obj.~T ();
    }

}; // end of master

template<typename T>
struct destruct_impl<T, // specialize to trap incorrect ("delete"-like) usage
    typename std::enable_if<std::is_pointer<T>::value>::type>
{
    static VR_NORETURN void evaluate (T & obj)
    {
        static_assert (lazy_false<T>::value, "util::destruct() invoked on a pointer");
    }

}; // end of specialization
//............................................................................

extern std::string
class_name (string_literal_t const mangled_name);

} // end of 'impl'
//............................................................................
//............................................................................

inline std::string
class_name (std::type_info const & ti)
{
    return impl::class_name (ti.name ());
}

inline std::string
class_name (std::type_index const & tix)
{
    return impl::class_name (tix.name ());
}
//............................................................................
/**
 * @return demangled name of 'obj' type, which could be different from (e.g. a subclass of) 'T'
 *
 * @note on itanium ABI compilers (Linux gcc/icc/clang) this works with all classes, not just
 *       polymorphic ones
 */
template<typename T>
std::string
class_name (const T & obj)
{
    return class_name (typeid (obj));
}

/**
 * @return demangled name of 'T'
 */
template<typename T>
std::string
class_name ()
{
    return class_name (typeid (T));
}
//............................................................................

template<typename T>
std::string
instance_name (T const * const obj_ptr)
{
    return (obj_ptr ? ('{' + class_name (* obj_ptr) + '@' + hex_string_cast (obj_ptr) + '}') : "null");
}
//............................................................................
/**
 * @return 'true' if 'obj' was default-constructed, 'false' if 'T' isn't default-constructible
 */
template<typename T>
bool
default_construct_if_possible (T & obj)
{
    return impl::default_construct_impl<T>::evaluate (obj);
}

template<typename T>
void
destruct (T & obj)
{
    impl::destruct_impl<T>::evaluate (obj);
}

} // end of 'util'
//............................................................................
//............................................................................
/*
 * a short name equivalent of 'class_name<T> ()' for using in debug macros
 */
template<typename T>
std::string
cn_ ()                      { return util::class_name<T> (); }

} // end of namespace
//----------------------------------------------------------------------------
