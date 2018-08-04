#pragma once

#include "vr/asserts.h"
#include "vr/util/classes.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
template<typename T>
class singleton_constructor; // forward

//............................................................................
//............................................................................
namespace impl
{

template<typename T, typename T_FACTORY, typename = void>
struct mark_constructed_impl
{
    static VR_FORCEINLINE void evaluate (T_FACTORY & f, T * const obj)
    {
    }

}; // end of master

template<typename T, typename T_FACTORY>
struct mark_constructed_impl<T, T_FACTORY,
    typename util::enable_if_t<std::is_base_of<singleton_constructor<T>, T_FACTORY>::value> >
{
    static VR_FORCEINLINE void evaluate (T_FACTORY & f, T * const obj)
    {
        f.m_obj = obj;
    }

}; // end of specialization
//............................................................................

template<typename T, typename T_FACTORY>
class factory_holder final
{
    public: // ...............................................................

        factory_holder (T * const obj) :
            m_factory { obj }
        {
            mark_constructed_impl<T, T_FACTORY>::evaluate (m_factory, obj); // must be done *after* 'm_factory' construction
        }

    private: // ..............................................................

        T_FACTORY m_factory; // aggregating allows 'T_FACTORY' to be final

}; // end of class

} // end of 'impl'
//............................................................................
//............................................................................
/**
 * @note this factory can be derived by a class that knows how to construct 'T';
 *       only the constructor needs to be provided in that case
 *
 * @note in-place construction/destruction
 */
template<typename T>
class singleton_constructor
{
    public: // ...............................................................

        VR_ASSUME_COLD singleton_constructor () = default;

        VR_ASSUME_COLD ~singleton_constructor ()
        {
            if (m_obj) util::destruct (* m_obj);
        }

    private: // ..............................................................

        template<typename, typename, typename> friend class impl::mark_constructed_impl; // grant access to 'm_obj'
        template<typename, typename> friend class impl::factory_holder; // grant access to the constructor below

        /*
         * non-public/non-inheritable constructor that does default construction
         * and is used when 'singleton' defaults on 'T_FACTORY'
         */
        VR_ASSUME_COLD singleton_constructor (T * const obj)
        {
            bool const rc = util::default_construct_if_possible (* obj);
            check_condition (rc, cn_<T> ());
        }


        T * m_obj { }; // will be nonnull only if subclass constructor succeeds

}; // end of class
//............................................................................

template<typename T, typename T_FACTORY = singleton_constructor<T> > // 'T_FACTORY' also tags
class singleton final: noncopyable
{
    public: // ...............................................................

        /*
         * note: this simple impl is MT-safe in c++11
         */
        static T & instance ()
        {
            T * const obj = reinterpret_cast<T *> (& m_obj);

            static impl::factory_holder<T, T_FACTORY> g_fh { obj }; // TODO handle exceptions

            return (* obj);
        }

    private: // ..............................................................

        using storage_type          = typename std::aligned_storage<sizeof (T), alignof (T)>::type;

        static storage_type m_obj;

}; // end of class

template<typename T, typename T_FACTORY>
typename singleton<T, T_FACTORY>::storage_type singleton<T, T_FACTORY>::m_obj;

//............................................................................

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
