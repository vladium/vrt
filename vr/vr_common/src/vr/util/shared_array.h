#pragma once

#include "vr/util/classes.h" // util::destruct()
#include "vr/util/memory.h"
#include "vr/util/shared_object.h"

#include <limits>

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
//............................................................................
//............................................................................
namespace impl
{

template<typename MT_POLICY>
class shared_array_base: public impl::shared_base<MT_POLICY>
{
    protected: // ............................................................

        using base              = impl::shared_base<MT_POLICY>;

    public: // ...............................................................

        using raw_ref_count_t   = typename base::raw_ref_count_t;

        // ACCESSORs:

        VR_FORCEINLINE const int32_t & size () const
        {
            return m_size;
        }

        /*
         * an overload that helps with disambiguation when 'BASE' declares its own size
         */
        VR_FORCEINLINE const int32_t & array_size () const
        {
            return size ();
        }

    protected: // ............................................................

        using ref_count_t       = typename base::ref_count_t;


        int32_t m_size; // initialized by 'create()'

}; // end of class

} // end of 'impl'
//............................................................................
//............................................................................
/**
 */
template<typename T, typename MT_POLICY = thread::safe>
class shared_array final: public impl::shared_array_base<MT_POLICY>,
                          noncopyable
{
    private: // ..............................................................

        using array_base        = impl::shared_array_base<MT_POLICY>;
        using this_type         = shared_array<T, MT_POLICY>;

        using typename array_base::ref_count_t;

    public: // ...............................................................

        using ptr               = boost::intrusive_ptr<this_type>;

        template<typename ... ARGs>
        static typename this_type::ptr create (int32_t const size, ARGs && ... args)
        {
            signed_size_t const alloc_size = sizeof (data_offset_calc) + (size - 1) * sizeof (T);

            void * const mem = impl::allocate_storage (size, alignof (data_offset_calc), alloc_size); // throws on alloc failure
            this_type * const obj = static_cast<this_type *> (mem);

            new (& obj->m_ref_count) ref_count_t { 0 }; // construct 'm_ref_count'
            obj->m_size = size;

            if (! util::is_trivially_constructible<T, ARGs ...>::value) // compile-time branch
            {
                for (int32_t i = 0; i < size; ++ i) // construct array slots
                {
                    new (& obj->data ()[i]) T { std::forward<ARGs> (args) ... };
                }
            }

            return { obj };
        }

        // ACCESSORs:

        T const * data () const
        {
            return reinterpret_cast<const T *> (static_cast<const int8_t *> (static_cast<const void *> (this)) + data_offset);
        }

        // MUTATORs:

        T * data ()
        {
            return const_cast<T *> (const_cast<const this_type *> (this)->data ());
        }

        // intrusive_ptr support:

        friend VR_FORCEINLINE void intrusive_ptr_add_ref (this_type * const obj)
        {
            ++ obj->m_ref_count;
        }

        friend VR_FORCEINLINE void intrusive_ptr_release (this_type * const obj)
        {
            if (! (-- obj->m_ref_count)) delete_array (obj);
        }

    private: // ..............................................................

        struct data_offset_calc: public array_base // 'm_ref_count', 'm_size'
        {
            T _data; // first array slot

        }; // end of nested class

        // this fails due to uniformity of access control requirements:
        //  static_assert (std::is_standard_layout<align_calc>::value, "design assumption violated for 'T'");
        // so:

    VR_DIAGNOSTIC_PUSH ()
    VR_DIAGNOSTIC_IGNORE ("-Winvalid-offsetof")
        static const std::size_t data_offset    = offsetof (data_offset_calc, _data);
    VR_DIAGNOSTIC_POP ()


        static void delete_array (this_type * const obj)
        {
            // destruct array slots (in reverse order of construction):

            if (! std::is_trivially_destructible<T>::value) // compile-time branch
            {
                for (int32_t i = obj->m_size; -- i >= 0; )
                {
                    try
                    {
                        util::destruct (obj->data ()[i]);
                    }
                    catch (...) { }
                }
            }

            // destruct 'm_ref_count' (might be trivial):

            util::destruct (obj->m_ref_count);

            // free memory:

            util::aligned_free (obj);
        }

}; // end of class

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
