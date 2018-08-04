#pragma once

#include "vr/asserts.h"

#include <memory>

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
/**
 */
class resizable_buffer final // movable
{
    public: // ...............................................................

        using size_type             = int64_t; // might want to make this a type parameter at some point


        resizable_buffer (size_type const initial_capacity = 0);

        resizable_buffer (resizable_buffer && rhs) = default;

        // ACCESSORs:

        size_type const & capacity () const
        {
            return m_capacity;
        }

        int8_t const * data () const
        {
            return m_buf.get ();
        }

        // MUTATORs:

        int8_t * data ()
        {
            return const_cast<int8_t *> (const_cast<resizable_buffer const *> (this)->data ());
        }

        /**
         * @return same data pointer as @ref data() [whether existing or re-allocated to accomodate new capacity]
         */
        VR_FORCEINLINE int8_t * ensure_capacity (size_type const capacity)
        {
            assert_positive (capacity);

            // make the frequent branch inlinable:

            if (VR_LIKELY (capacity <= m_capacity))
                return data ();

            return increase_capacity (capacity);
        }

    private: // ..............................................................

        VR_ASSUME_HOT int8_t * increase_capacity (size_type const capacity); // outlined


        std::unique_ptr<int8_t []> m_buf; // TODO use large obj allocator (align on page boundary, pool, etc)
        size_type m_capacity;

}; // end of class

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
