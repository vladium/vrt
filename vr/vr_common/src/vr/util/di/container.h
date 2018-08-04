#pragma once

#include "vr/enums.h"
#include "vr/settings.h"
#include "vr/startable.h"
#include "vr/util/classes.h"
#include "vr/util/di/component.h"
#include "vr/util/di/lifecycle_exception.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
namespace di
{
class container_barrier; // forward

/**
 * TODO to support active components and NUMA efficiency, add() component factories, not already created instances
 * TODO make start()/stop() MT-safe?
 *
 * @note a 'container' is itself a 'component'
 */
class container: public component, public startable,
                 noncopyable
{
    private: // ..............................................................

        struct configurator final: noncopyable
        {
                configurator & operator() (std::string const & name, component * const c);

            private: friend class container;

                configurator (container &);

                container & m_parent;

        }; // end of nested class

        struct proxy final: noncopyable
        {
                template<typename T>
                operator T & () const;

            private: friend class container;

                proxy (component & c);

                component * const m_obj;

        }; // end of nested class

    public: // ...............................................................

        VR_ASSUME_COLD container (std::string const & name = { }, settings const & cfg = nullptr);
        VR_ASSUME_COLD ~container () VR_NOEXCEPT; // calls 'stop ()' and logs anything thrown

        // startable:

        VR_ASSUME_COLD void start () override;
        VR_ASSUME_COLD void stop () override; // idempotent

        // ACCESSORs:

        std::string const & name () const;

        proxy operator[] (std::string const & name) const;

        std::string const & name_of (component * const c) const;

        // MUTATORs:

        configurator configure (); // syntactic sugar for repeated 'add ()'s
        container & add (std::string const & name, component * const c);

        // interface for dealing with 'steppable' components:

        bool run_for (timestamp_t const duration);
        bool run_until (timestamp_t const time_utc);


        template<typename A> class access_by;

    private: // ..............................................................

        friend class container_barrier; // grant access to 'report_failure()'
        template<typename A> friend class access_by;

        class pimpl; // forward

        /*
         * note: this can be invoked from threads other than the container's start()/stop() thread
         */
        void report_failure (bool const in_start, std::exception_ptr const eptr) VR_NOEXCEPT;

        void request_stop () VR_NOEXCEPT; // TODO the externally-requested stop protocol needs more work
        void report_step_failure (std::exception_ptr const eptr) VR_NOEXCEPT;

        std::unique_ptr<pimpl> m_impl;
        std::string const m_name;

}; // end of class
//............................................................................

template<typename T>
container::proxy::operator T & () const
{
    T * const r = dynamic_cast<T *> (m_obj);
    if (VR_UNLIKELY (! r))
        throw_x (type_mismatch, "instance of {" + class_name (* m_obj) + "} can't be cast to {" + class_name<T> () + '}');

    return (* r);
}

} // end of 'di'
} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
