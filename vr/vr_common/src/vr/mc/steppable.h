#pragma once

#include "vr/macros.h" // VR_ASSUME_HOT

//----------------------------------------------------------------------------
namespace vr
{
namespace mc
{
class step_runnable; // forward
class step_ctl; // forward
/**
 */
class steppable
{
    public: // ...............................................................

        virtual ~steppable ()   = default;

        virtual VR_ASSUME_HOT void step ()  = 0;

    protected: // ............................................................

        /**
         * inform the "powers that be" that we'd like to stop running
         *
         * it is sufficient to invoke this method once, but it is not a logic error
         * to do so multiple times (e.g. due to nesting)
         */
        VR_ASSUME_COLD void request_stop () VR_NOEXCEPT;

    private: // ..............................................................

        friend class step_runnable;

        step_ctl * m_step_ctl { }; // this is guaranteed to be set before the first 'step()'

}; // end of class
//............................................................................

template<typename ... ASPECTs>
struct steppable_: public steppable, public ASPECTs ...
{
    using steppable::steppable; // make sure to inherit constructors

}; // end of class
//............................................................................

} // end of 'mc'
} // end of namespace
//----------------------------------------------------------------------------
