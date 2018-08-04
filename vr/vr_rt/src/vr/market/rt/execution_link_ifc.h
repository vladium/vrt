#pragma once

#include "vr/market/rt/cfg/defs.h" // symbol_liid_relation
#include "vr/mc/lf_spsc_buffer.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
//............................................................................
//............................................................................
namespace impl
{

constexpr int32_t request_buffer_capacity ()    { return 32; } // TODO profile lat = f(capacity) in *prod*

//............................................................................

class execution_link_ifc_base
{
    public: // ...............................................................

        // ACCESSORs:

        VR_FORCEINLINE bitset32_t const & partition_mask () const
        {
            return m_partition_mask;
        }

        VR_FORCEINLINE char otk_prefix () const
        {
            return m_otk_prefix;
        }

    protected: // ............................................................

        execution_link_ifc_base ()  = default;

        execution_link_ifc_base (bitset32_t const partition_mask, uint32_t const otk_prefix) :
            m_partition_mask { partition_mask },
            m_otk_prefix { otk_prefix }
        {
            assert_nonzero (m_partition_mask);
            assert_in_inclusive_range (m_otk_prefix, 0x0, 0xF);
        }


        bitset32_t m_partition_mask { }; // partitions that are relevant to *this* agent
        uint32_t m_otk_prefix { }; // [in ['A', 'Z'] range] prefix for uniqizing *this* agent's 'order_token' value space

}; // end of class

} // end of 'impl'
//............................................................................
//............................................................................
/**
 */
template<typename ORDER_REQUEST>
class execution_link_ifc final: public impl::execution_link_ifc_base
{
    private: // ..............................................................

        using super         = impl::execution_link_ifc_base;

    public: // ...............................................................

        using request_queue = mc::lf_spsc_buffer<ORDER_REQUEST, impl::request_buffer_capacity ()>;


        execution_link_ifc ()   = default;

        // TODO make this one non-public
        execution_link_ifc (bitset32_t const partition_mask, uint32_t const otk_prefix, request_queue & queue) :
            super (partition_mask, otk_prefix),
            m_queue { & queue }
        {
        }


        VR_FORCEINLINE request_queue & queue ()
        {
            assert_nonnull (m_queue); // non-default constructor has been called

            return (* m_queue);
        }

    private: // ..............................................................

        request_queue * m_queue { };

}; // end of class

} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
