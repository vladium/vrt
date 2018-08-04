#pragma once

#include "vr/asserts.h"
#include "vr/containers/util/stable_vector.h"
#include "vr/runnable.h"
#include "vr/startable.h"
#include "vr/util/classes.h"
#include "vr/mc/bound_runnable.h"
#include "vr/util/type_traits.h"

#include <boost/thread/thread.hpp>

#include <typeindex>

//----------------------------------------------------------------------------
namespace vr
{
namespace test
{
class task; // forward
class task_container; // forward

//............................................................................
//............................................................................
namespace impl
{

template<typename F>
struct task_runnable final: mc::bound_runnable
{
    private: // ..............................................................

        using bound     = mc::bound_runnable;

    public: // ...............................................................

        task_runnable (F const & f, int32_t const PU = -1) :
            bound (PU),
            m_f { f }
        {
        }

        task_runnable (F && f, int32_t const PU = -1) :
            bound (PU),
            m_f { std::forward<F> (f) }
        {
        }

        // runnable:

        void operator() () final override
        {
            try
            {
                bound::bind ();

                m_f ();
            }
            catch (boost::thread_interrupted const &)
            {
                throw; // re-throw
            }
            catch (std::exception const &)
            {
                // TODO
            }

            bound::unbind (); // destructor always calls, but try to unbind eagerly
        }


        F m_f;

}; // end of class

} // end of 'impl'
//............................................................................
//............................................................................

class task final: public startable
{
    public: // ...............................................................

        // TODO make sure moving constructor is preferred when possible:

        template<typename F>
        task (F const & f, int32_t const PU = -1) :
            m_f_runnable { new impl::task_runnable<F> { f, PU } }
        {
        }

        template<typename F>
        task (F && f, int32_t const PU = -1,
            util::enable_if_t</* avoid stealing from lvalues: */std::is_rvalue_reference<decltype (f)>::value> * const = nullptr) :
            m_f_runnable { new impl::task_runnable<F> { std::forward<F> (f), PU } }
        {
        }

        // ACCESSORs:

        std::string const & name () const;

        template<typename F>
        operator F & () const
        {
            using F_bare    = std::decay_t<F>;

            impl::task_runnable<F_bare> const * const tr = dynamic_cast<impl::task_runnable<F_bare> const *> (m_f_runnable.get ());
            if (VR_UNLIKELY (! tr))
                throw_x (type_mismatch, "this task does not wrap an instance {" + util::class_name<F_bare> () + '}');

            return (* const_cast<F *> (static_cast<F_bare const *> (& tr->m_f)));
        }

        // MUTATORs:

        // startable:

        void start () final override;
        void stop () final override;

    private: // ..............................................................

        std::unique_ptr<mc::bound_runnable> m_f_runnable; // non-const to stay movable
        boost::thread m_f_thread { }; // create as not-a-thread

}; // end of class
//............................................................................

class task_container final: public startable,
                            noncopyable
{
    public: // ...............................................................

        task_container ()   = default;

        // ACCESSORs:

        task const & operator[] (int32_t const index) const;
        task const & operator[] (std::string const & name) const;

        // MUTATORs:

        // populate container with [optionally] named tasks:

        void add (task && t, std::string const & name = { });

        task & operator[] (int32_t const index);
        task & operator[] (std::string const & name);

        // startable:

        void start () final override;
        void stop () final override;

    private: // ..............................................................

        using task_name_map     = boost::unordered_map<std::string, task *>;

        util::stable_vector<task> m_tasks { };
        task_name_map m_name_map { }; // values reference [stable] entries of 'm_tasks'

}; // end of class
//............................................................................

inline task &
task_container::operator[] (int32_t const index)
{
    return const_cast<task &> (const_cast<task_container const *> (this)->operator[] (index));
}

inline task &
task_container::operator[] (std::string const & name)
{
    return const_cast<task &> (const_cast<task_container const *> (this)->operator[] (name));
}

} // end of 'test'
} // end of namespace
//----------------------------------------------------------------------------
