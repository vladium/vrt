#pragma once

#include "vr/mc/cache_aware.h"
#include "vr/mc/mc.h"
#include "vr/util/type_traits.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace mc
{
//............................................................................
//............................................................................
namespace impl
{

VR_EXPLICIT_ENUM (ctx_bits,
    bitset32_t,
        (0b01,  INVALID)
        (0b10,  COMMIT)
        (0b11,  mask)

); // end of enum
//............................................................................

template<typename T, int32_t CAPACITY>
class dequeue_result_impl; // forward

template<typename T, int32_t CAPACITY>
class enqueue_result_impl; // forward

//............................................................................

class lf_spsc_buffer_base
{
    public: // ...............................................................

        using size_type     = signed_size_t;

        // ACCESSORs:

        /*
         * @note this is purely for debugging (only meaningful in quiescent state)
         */
        size_type size () const
        {
            return (volatile_cast (m_p_ctx.value ().m_p_position) - volatile_cast (m_c_ctx.value ().m_c_position));
        }

    protected: // ............................................................

        template<typename T, int32_t CAPACITY> friend class dequeue_result_impl;
        template<typename T, int32_t CAPACITY> friend class enqueue_result_impl;

        vr_static_assert (std::is_signed<size_type>::value);

        struct consumer_context
        {
            size_type m_c_position { };
            size_type m_p_local { };

        }; // end of nested class

        struct producer_context
        {
            size_type m_p_position { };
            size_type m_c_local { };

        }; // end of nested class

        using cl_consumer_context   = cache_line_padded_field<consumer_context>;
        using cl_producer_context   = cache_line_padded_field<producer_context>;


        cl_consumer_context m_c_ctx { }; // [owned by C, read by P]
        cl_producer_context m_p_ctx { }; // [owned by P, read by C]

}; // end of class
//............................................................................

template<typename T, int32_t CAPACITY>
struct dequeue_result_impl
{
    VR_FORCEINLINE dequeue_result_impl (lf_spsc_buffer_base::cl_consumer_context * const ctx) :
        m_ctx { ctx }
    {
    }

    /*
     * release if valid
     */
    VR_FORCEINLINE ~dequeue_result_impl ()
    {
        compiler_fence ();

        std_uintptr_t const ctx_raw { reinterpret_cast<std_uintptr_t> (m_ctx) };

        if (VR_LIKELY (! (static_cast<bitset32_t> (ctx_raw) & ctx_bits::INVALID)))
        {
            lf_spsc_buffer_base::cl_consumer_context * const ctx { reinterpret_cast<lf_spsc_buffer_base::cl_consumer_context *> (ctx_raw) };

            ++ volatile_cast (ctx->value ().m_c_position);
        }
    }


    VR_FORCEINLINE explicit operator bool () const
    {
        return (! (static_cast<uint32_t> (reinterpret_cast<std_uintptr_t> (m_ctx)) & ctx_bits::INVALID));
    }

    VR_FORCEINLINE T const & value () const // TODO the only method that depends on CAPACITY
    {
        assert_condition (static_cast<bool> (* this), m_ctx);

        lf_spsc_buffer_base::cl_consumer_context * const ctx { m_ctx };

        return (* reinterpret_cast<T const *> (& reinterpret_cast<cache_line_slot const *> (ctx) [2 + (static_cast<uint32_t> (ctx->value ().m_c_position) & (CAPACITY - 1))]));
    }

    VR_FORCEINLINE operator T const & () const
    {
        return value ();
    }

    private:

        lf_spsc_buffer_base::cl_consumer_context * m_ctx; // note: use fully typed ptr to let compiler see the alignment

}; // end of class


template<typename T, int32_t CAPACITY>
struct enqueue_result_impl
{
    VR_FORCEINLINE enqueue_result_impl (lf_spsc_buffer_base::cl_producer_context * const ctx) :
        m_ctx { ctx }
    {
    }

    /*
     * release if valid and 'commit ()' has been called
     */
    VR_FORCEINLINE ~enqueue_result_impl ()
    {
        compiler_fence ();

        std_uintptr_t const ctx_raw { reinterpret_cast<std_uintptr_t> (m_ctx) };

        if (VR_LIKELY ((static_cast<bitset32_t> (ctx_raw) & ctx_bits::mask) == ctx_bits::COMMIT))
        {
            lf_spsc_buffer_base::cl_producer_context * const ctx { reinterpret_cast<lf_spsc_buffer_base::cl_producer_context *> ((ctx_raw >> 2) << 2) };

            ++ volatile_cast (ctx->value ().m_p_position);
        }
    }


    VR_FORCEINLINE explicit operator bool () const
    {
        return (! (static_cast<uint32_t> (reinterpret_cast<std_uintptr_t> (m_ctx)) & ctx_bits::INVALID));
    }

    VR_FORCEINLINE void commit ()
    {
        assert_condition (static_cast<bool> (* this), m_ctx);

        reinterpret_cast<std_uintptr_t &> (m_ctx) |= ctx_bits::COMMIT;
    }


    VR_FORCEINLINE T & value () // TODO the only method that depends on CAPACITY
    {
        assert_condition (static_cast<bool> (* this), m_ctx);

        lf_spsc_buffer_base::cl_producer_context * const ctx { m_ctx };

        return (* reinterpret_cast<T *> (& reinterpret_cast<cache_line_slot *> (ctx) [1 + (static_cast<uint32_t> (ctx->value ().m_p_position) & (CAPACITY - 1))]));
    }

    VR_FORCEINLINE operator T & ()
    {
        return value ();
    }

    private:

        lf_spsc_buffer_base::cl_producer_context * m_ctx; // note: use fully typed ptr to let compiler see the alignment

}; // end of class

} // end of 'impl'
//............................................................................
//............................................................................

template<typename T>
struct default_lf_spsq_buffer_options
{
    // TODO possibly useful traits: repeat counts for try_*() ops or blocking ops if/when added

}; // end of class
//............................................................................
/**
 * classic SPSC ring buffer that relies entirely on x86-TSO mem model and does
 * not need mfence or atomic ops for correctness; main customization here
 * is support for purely non-blocking operation for both consumer and producer
 * ends and scoped try/commit-or-abort API design;
 *
 * @note currently 'T' is statically asserted to be trivially constructible
 *
 * @note the impl is statically biased towards assuming a buffer is more frequently empty than full
 *
 * @note technically, this impl is "lock-free" and not "wait-free" but this
 *       distinction is not relevant when the use case is a polling loop
 */
template<typename T, int32_t CAPACITY, typename OPTIONS = default_lf_spsq_buffer_options<T>>
class lf_spsc_buffer final: public impl::lf_spsc_buffer_base
{
    private: // ..............................................................

        vr_static_assert (util::is_trivially_constructible<T>::value);
        vr_static_assert (sizeof (T) <= sys::cpu_info::cache::static_line_size ());

        vr_static_assert (vr_is_power_of_2 (CAPACITY));

        using super         = impl::lf_spsc_buffer_base;

    public: // ...............................................................

        using super::size_type;
        using value_type        = T;

        using dequeue_result    = impl::dequeue_result_impl<T, CAPACITY>;
        using enqueue_result    = impl::enqueue_result_impl<T, CAPACITY>;


        lf_spsc_buffer ()
        {
            __builtin_memset (m_data, 0, sizeof (m_data)); // keep valgrind happy [not needed for correctness]
        }

        // MUTATORs:

        VR_FORCEINLINE dequeue_result try_dequeue (); // TODO 'spin_count' ?

        VR_FORCEINLINE enqueue_result try_enqueue (); // TODO 'spin_count' ?

    private: // ..............................................................

        using entry     = cache_line_padded_field<T>;

        entry m_data [CAPACITY];

}; // end of class
//............................................................................

template<typename T, int32_t CAPACITY, typename OPTIONS>
typename lf_spsc_buffer<T, CAPACITY, OPTIONS>::dequeue_result
lf_spsc_buffer<T, CAPACITY, OPTIONS>::try_dequeue () // force-inlined
{
    cl_consumer_context & c_ctx = m_c_ctx;

    size_type const c_pos = c_ctx.value ().m_c_position;
    size_type & p_local { c_ctx.value ().m_p_local };

    if (VR_UNLIKELY (c_pos >= p_local))
    {
        if (VR_UNLIKELY (c_pos >= (p_local = volatile_cast (m_p_ctx.value ().m_p_position)))) // note 'm_p_local' side-effect
            return { reinterpret_cast<cl_consumer_context *> (reinterpret_cast<std_uintptr_t> (& c_ctx) | impl::ctx_bits::INVALID) };
    }

    return { & c_ctx };
}
//............................................................................

template<typename T, int32_t CAPACITY, typename OPTIONS>
typename lf_spsc_buffer<T, CAPACITY, OPTIONS>::enqueue_result
lf_spsc_buffer<T, CAPACITY, OPTIONS>::try_enqueue () // force-inlined
{
    cl_producer_context & p_ctx = m_p_ctx;

    size_type const p_pos_m_cap = (p_ctx.value ().m_p_position - CAPACITY);
    size_type & c_local { p_ctx.value ().m_c_local };

    if (VR_UNLIKELY (p_pos_m_cap >= c_local))
    {
        if (VR_UNLIKELY (p_pos_m_cap >= (c_local = volatile_cast (m_c_ctx.value ().m_c_position)))) // note 'm_c_local' side-effect
            return { reinterpret_cast<cl_producer_context *> (reinterpret_cast<std_uintptr_t> (& p_ctx) | impl::ctx_bits::INVALID) };
    }

    return { & p_ctx };
}

} // end of 'mc'
} // end of namespace
//----------------------------------------------------------------------------
