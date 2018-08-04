#pragma once

#include "vr/mc/cache_aware.h"
#include "vr/mc/spinflag.h"
#include "vr/runnable.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace mc
{
/**
 * a support base class for implementing PU-bound runnables
 */
class VR_ALIGNAS_CL bound_runnable: public runnable
{
    public: // ...............................................................

        using stopflag_t    = mc::spinflag</* CACHE_LINE_ALIGN */false>;


        virtual ~bound_runnable () VR_NOEXCEPT; // calls 'unbind()'

        // ACCESSORs:

        int32_t const & PU () const
        {
            return m_PU;
        }

        std::string const & name () const
        {
            return m_name;
        }

        VR_FORCEINLINE stopflag_t const & stop_flag () const VR_NOEXCEPT
        {
            return m_stop_flag;
        }

        template<typename A> class access_by;

    protected: // ............................................................

        bound_runnable (int32_t const PU, std::string const & name = { });

        // ACCESSORs:

        /**
         * @return 'true' iff 'm_PU' is non-negative and 'm_saved_affinity' is non-empty
         */
        bool bound () const; // checked to make '[un]bound ()' idempotent

        // MUTATORs:

        int32_t const & bind (); // idempotent
        void unbind () VR_NOEXCEPT; // idempotent

        VR_ASSUME_COLD stopflag_t & stop_flag () VR_NOEXCEPT;

    private: // ..............................................................

        template<typename A> friend class access_by;

        // normally, we'd isolate a variable like 'm_stop_flag' in its own cache line
        // to absolutely guarantee no false sharing no matter where it is allocated
        // or how it is embedded as a field; however, here we embed it with other
        // read-only fields and force alignment at the entire class level instead:

        /* [vptr] */
        stopflag_t m_stop_flag { };     // read often (spin), written once
        int32_t const m_PU;             // [negative means "not actually PU-bound"]
        std::string m_name;             // optionally set in constructor, otherwise reset in 'bind ()'
        std::unique_ptr<bit_set> m_saved_affinity { };  // set once by the 'bind ()' call that succeeds

}; // end of class

} // end of 'mc'
} // end of namespace
//----------------------------------------------------------------------------
