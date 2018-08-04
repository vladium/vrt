
#include "vr/util/di/container.h"

#include "vr/mc/step_runnable.h" // TODO cyclic dep on 'mc::'
#include "vr/mc/waitable.h" // TODO cyclic dep on 'mc::'
#include "vr/util/di/container_barrier.h"
#include "vr/util/logging.h"

#include <boost/bimap.hpp>
#include <boost/bimap/set_of.hpp>
#include <boost/bimap/unordered_set_of.hpp>
#include <boost/pending/disjoint_sets.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>

#include <algorithm>

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
namespace di
{
//............................................................................

template<>
struct container::access_by<mc::step_ctl>
{
    static VR_FORCEINLINE void request_stop (container & c) VR_NOEXCEPT
    {
        c.request_stop ();
    }

    static VR_FORCEINLINE void report_step_failure (container & c, std::exception_ptr const & eptr) VR_NOEXCEPT
    {
        c.report_step_failure (eptr);
    }

}; // end of specialization
//............................................................................
//............................................................................
namespace
{

VR_ENUM (state,
    (
        created,
        configured,
        resolved,   // TODO rm?
        started,
        stopped
    ),
    printable

); // end of enum
//............................................................................

using component_bimap   = boost::bimap
                        <
                            boost::bimaps::set_of<std::string>,             // keep ordered for name-based iteration
                            boost::bimaps::unordered_set_of<component *>    // don't allow aliasing (for now)
                        >;
//............................................................................

struct PU_affinity_group
{
    boost::unordered_set<std::string> m_members { }; // actual components names or aliases/logical thread names
    int32_t m_PU { std::numeric_limits<int32_t>::min () }; // >= 0: strict assignment to indicated PU, < -1: strict assignment to a PU chosen by container, -1: not bound

}; // end of class

using PU_affinity_group_map = boost::unordered_map</* PU **/int32_t, PU_affinity_group>;

//............................................................................

struct component_PU_affinity_group
{
    int32_t const m_PU;
    std::string m_name { };
    std::vector<mc::steppable *> m_steps { }; // [non-owning]

}; // end of class

struct PU_task final
{
    PU_task (std::unique_ptr<mc::step_runnable> && runnable) :
        m_runnable { std::move (runnable) }
    {
    }

    std::unique_ptr<mc::step_runnable> m_runnable { }; // non-const to stay movable
    boost::thread m_thread { }; // create as not-a-thread

}; // end of class
//............................................................................

struct step_ctl_impl final: public mc::step_ctl // TODO easier for 'container' to implement 'step_ctl' directly?
{
    step_ctl_impl (container & parent, std::size_t const latch_count, container_barrier::turn_map && turns) :
        m_parent { parent },
        m_sequencer { parent, latch_count, std::move (turns) }
    {
    }

    // step_ctl:

    void request_stop (mc::steppable const * const requestor) VR_NOEXCEPT final override
    {
        // TODO proper impl

        if (requestor) LOG_trace1 << "\tstop request from " << util::instance_name (& requestor);
        LOG_trace1 << "NOT IMPLEMENTED YET FULLY: step_ctl_impl::request_stop()";

        container::access_by<mc::step_ctl>::request_stop (m_parent);
    }

    void report_failure (std::exception_ptr const & eptr) VR_NOEXCEPT final override
    {
        container::access_by<mc::step_ctl>::report_step_failure (m_parent, eptr);
    }

    container_barrier & sequencer () final override
    {
        return m_sequencer;
    }


    container & m_parent;
    container_barrier m_sequencer;

}; // end of class
//............................................................................

void
parse_affinity_cfg (settings const & cfg, PU_affinity_group_map & out)
{
    LOG_trace3 << "parsing affinity configuration ...";

    check_condition (cfg.is_object (), cfg.type ());

    using ID_t          = int32_t;

    boost::unordered_map</* set ID */ID_t, PU_affinity_group> PU_groups { }; // populated below

    // map 'cfg' entries to int IDs (also verify key uniqueness):
    {
        std::vector<json> elements { }; // indexed by ID
        std::vector<std::string> names { }; // parallel to 'elements'

        boost::unordered_map<std::string, ID_t> name_map { };
        boost::unordered_map</* PU **/int32_t, ID_t> PU_map { }; // direct specs only, not "aliases"

        for (auto i = cfg.begin (); i != cfg.end (); ++ i) // need to use special iterator for json::objects
        {
            std::string const & name = i.key ();
            ID_t const ID = elements.size ();

            if (VR_UNLIKELY (! name_map.emplace (name, ID).second))
                throw_x (invalid_input, "duplicate affinity mapping for " + print (name));

            if (i.value ().is_number ()) // direct PU mapping
            {
                int32_t const PU = i.value ().get<int32_t> ();
                PU_map.emplace (PU, ID); // ok if this PU has been mapped already
            }

            elements.push_back (i.value ());
            names.push_back (name);
        }
        assert_eq (elements.size (), name_map.size ());

        ID_t const element_count = elements.size ();

        std::unique_ptr<ID_t []> const rank { std::make_unique<ID_t []> (element_count) };
        std::unique_ptr<ID_t []> const parent { std::make_unique<ID_t []> (element_count) };

        boost::disjoint_sets<ID_t *, ID_t *> ds { rank.get (), parent.get () };

        // create equivalence classes (each class maps to a unique PU assignment):

        for (ID_t id = 0; id < element_count; ++ id)
        {
            ds.make_set (id);
        }

        for (ID_t id = 0; id < element_count; ++ id)
        {
            json const & v = elements [id];

            ID_t id_equiv;

            if (v.is_number ()) // direct PU mapping
            {
                int32_t const PU = v.get<int32_t> ();

                id_equiv = PU_map.find (PU)->second;
            }
            else
            {
                check_eq (v.type (), json::value_t::string);

                auto const ni = name_map.find (v.get<std::string> ());
                if (VR_UNLIKELY (ni == name_map.end ()))
                    throw_x (invalid_input, "no alias or PU assignment rule for " + print (v.get<std::string> ()));

                id_equiv = ni->second;
            }

            ds.union_set (id, id_equiv);
        }

        // collect members into their sets and figure out PU assignments:

        for (ID_t id = 0; id < element_count; ++ id)
        {
            ID_t const set_id = ds.find_set (id);

            auto gi = PU_groups.find (set_id);
            if (gi == PU_groups.end ())
            {
                gi = PU_groups.emplace (set_id, PU_affinity_group { }).first;
            }

            PU_affinity_group & g { gi->second };

            g.m_members.insert (names [id]);

            json const & v = elements [id];
            if (v.is_number ()) // direct PU mapping
            {
                if (g.m_PU == std::numeric_limits<int32_t>::min ())
                    g.m_PU = v.get<int32_t> ();
                else
                    check_eq (g.m_PU, v.get<int32_t> ());
            }
        }
    }

    LOG_trace2 << "parsed affinity settings for " << PU_groups.size () << " distinct PU(s)";

    // it is possible that a set exists with no actual PU assignment (an alias cycle),
    // check for that:

    for (auto const & kv : PU_groups)
    {
        PU_affinity_group const & g = kv.second;

        LOG_trace3 << "  PU [" << g.m_PU << "] := " << print (g.m_members);

        if (VR_UNLIKELY (g.m_PU == std::numeric_limits<int32_t>::min ()))
            throw_x (invalid_input, "no PU assignment within alias group " + print (g.m_members) + " in settings\n" + print (cfg));
    }

    // 'PU_groups' is valid, but still keyed on the (useless from this point on) set ID,
    // move the data into a PU-based map:

    for (auto & kv : PU_groups)
    {
        PU_affinity_group & g = kv.second;
        int32_t const PU = g.m_PU;

        out.emplace (PU, std::move (g));
    }

    // [last use of 'PU_groups']
}

} // end of anonymous
//............................................................................
//............................................................................

container::configurator::configurator (container & parent) :
    m_parent { parent }
{
}

container::configurator &
container::configurator::operator() (std::string const & name, component * const c)
{
    m_parent.add (name, c);

    return (* this);
}
//............................................................................
//............................................................................

container::proxy::proxy (component & c) :
    m_obj { & c }
{
}
//............................................................................
//............................................................................

struct container::pimpl final
{
    VR_ASSUME_COLD pimpl (settings const & cfg)
    {
        if (! cfg.is_null ())
        {
            parse_affinity_cfg (cfg, m_PU_group_cfg);
        }
    }

    VR_ASSUME_COLD ~pimpl () VR_NOEXCEPT_IF (VR_RELEASE) // conditioned on build to allow failure testing in debug builds
    {
        for (int32_t i = m_dep_order.size (); -- i >= 0; ) // note: stop in inverse order of starting
        {
            component * const c = m_dep_order [i];
            assert_nonnull (c);

            std::string const & c_name = name_of (c);

            try
            {
                LOG_trace2 << "\tdestructing " << print (c_name) << ' ' << instance_name (c) << " ...";
                delete c;
            }
            catch (std::exception const & e)
            {
                LOG_error << "component " << print (c_name) << " FAILED in destructor: " << exc_info (e);
                break;
            }
            catch (...)
            {
                LOG_error << "component " << print (c_name) << " FAILED in destructor: unknown exception";
                break;
            }
        }
    }


    VR_ASSUME_COLD void binding_traversal ()
    {
        check_empty (m_dep_order);

        boost::unordered_set<component *> visited { };

        for (auto const & kv : m_components.right)
        {
            component * const c = kv.first;

            if (! visited.count (c))
            {
                std::string const & c_name = kv.second;

                bind_visit (c, c_name, m_dep_order, visited);
            }
        }

        m_dep_order.shrink_to_fit ();

        if (LOG_trace_enabled (2))
        {
            std::stringstream ss { }; ss << "dep order:\n";

            for (component * c : m_dep_order)
            {
                ss << '\t' << print (name_of (c)) << '\n';
            }
            LOG_trace2 << ss.str ();
        }
    }

    VR_ASSUME_COLD void start (container & parent)
    {
        // because we'll need to keep dep order within PU groups (for correct lifecycle method invocation
        // order), build a map to dep indices:

        boost::unordered_map<mc::steppable *, /* dep ix */int32_t> dep_index_map { };
        container_barrier::turn_map start_turns { }; // all 'startable's

        // pass 1: scan 'm_dep_order' for 'startable's and 'steppable's:
        {
            for (int32_t dep_index = 0, dep_index_limit = m_dep_order.size (); dep_index < dep_index_limit; ++ dep_index)
            {
                component * const c = m_dep_order [dep_index];

                startable * const cs = dynamic_cast<startable *> (c);
                mc::steppable * const cstep = dynamic_cast<mc::steppable *> (c);

                if (cstep) dep_index_map.emplace (cstep, dep_index);

                if (cs) start_turns.emplace (cs, static_cast<int32_t> (start_turns.size ()));
            }
        }

        std::vector<component_PU_affinity_group> component_groups { }; // like 'm_PU_group_cfg' except all names are actual components and groups are non-empty
        int32_t cfg_steppable_count { };

        if (! dep_index_map.empty ())
        {
            // pass 2: populate 'component_groups' with names that are actual components in this container
            {
                for (auto const & kv : m_PU_group_cfg)
                {
                    PU_affinity_group const & g = kv.second;
                    component_PU_affinity_group component_g { g.m_PU };

                    for (std::string const & name : g.m_members)
                    {
                        // see if 'name' is a component in this container:

                        auto const i = m_components.left.find (name);
                        if (i != m_components.left.end ())
                        {
                            component * const c = i->second;

                            // check if 'c' is actually 'steppable:

                            mc::steppable * const cs = dynamic_cast<mc::steppable *> (c);

                            if (VR_UNLIKELY (cs == nullptr)) // this is not fatal, but issue a warning for now
                                LOG_warn << "affinity cfg refers to " << print (name) << " component, but it is not 'steppable': " << instance_name (c);
                            else
                            {
                                assert_condition (dep_index_map.count (cs));
                                component_g.m_steps.push_back (cs);
                            }
                        }
                    }

                    if (! component_g.m_steps.empty ())
                    {
                        cfg_steppable_count += component_g.m_steps.size ();
                        component_groups.emplace_back (std::move (component_g)); // note: last use of 'component_g'
                    }
                }
                assert_le (cfg_steppable_count, signed_cast (dep_index_map.size ())); // by construction

                // if not all 'steppable's were referenced in the affinity cfg, it could be treated
                // as a config error (the case for now) or by assigning them to a "default PU" that would
                // come from the cfg:

                if (cfg_steppable_count < signed_cast (dep_index_map.size ()))
                    throw_x (illegal_state, "not all " + string_cast (dep_index_map.size ()) + " steppable component(s) have an affinity cfg");

                // TODO 'm_PU_group_cfg' won't be needed again
            }

            // pass 3: order individual steps within component groups consistently with 'm_dep_order':

            for (component_PU_affinity_group & cg : component_groups)
            {
                std::vector<mc::steppable *> & steps = cg.m_steps;
                assert_nonempty (steps);

                std::sort (steps.begin (), steps.end (), [& dep_index_map](mc::steppable * const lhs, mc::steppable * const rhs)
                   {
                        return (dep_index_map.find (lhs)->second < dep_index_map.find (rhs)->second);
                   });

                // finally, give 'cg' a synthetic name:

                if (signed_cast (steps.size ()) == 1)
                    cg.m_name = name_of (dynamic_cast<component *> (steps.front ()));
                else
                {
                    std::stringstream ns { };
                    for (int32_t s = 0, s_limit = steps.size (); s < s_limit; ++ s)
                    {
                        if (s) ns << '|';
                        ns << name_of (dynamic_cast<component *> (steps [s]));
                    }
                    cg.m_name = ns.str ();
                }
            }
        }


        if (component_groups.empty ()) // TODO rm this branch once the below is solid for all situations
        {
            for (int32_t i = 0, i_limit = m_dep_order.size (); i < i_limit; ++ i, ++ m_last_started) // note: the one that possibly threw is included
            {
                component * const c = m_dep_order [i];
                startable * const c_startable = dynamic_cast<startable *> (c);

                std::string const & c_name = name_of (c);

                if (c_startable)
                {
                    try
                    {
                        LOG_trace2 << "\tstarting " << print (c_name) << ' ' << instance_name (c) << " ...";
                        c_startable->start ();
                        LOG_trace2 << "\tstarted " << print (c_name) << ' ' << instance_name (c);
                    }
                    catch (std::exception const & e)
                    {
                        throw_x (app_exception, "component " + print (c_name) + " FAILED to start: " + string_cast (exc_info (e)));
                    }
                    catch (...)
                    {
                        throw_x (app_exception, "component " + print (c_name) + " FAILED to start: unknown exception");
                    }
                }
            }
        }
        else // we have component PU groups, populate and start'm_PU_tasks':
        {
            // if we have some 'startable's, create a 'step_ctl':

            m_step_ctl = std::make_unique<step_ctl_impl> (parent, 1 + component_groups.size (), std::move (start_turns)); // note: last used of 'turns'
            container_barrier & seq = m_step_ctl->m_sequencer;

            for (component_PU_affinity_group const & cg : component_groups)
            {
                m_PU_tasks.emplace_back (mc::step_runnable::create (cg.m_PU, cg.m_name, * m_step_ctl, cg.m_steps));
            }

            // move-assign real threads to 'm_PU_tasks':
            {
                // [don't make these thread inherit the current thread's affinity set]

                sys::affinity::scoped_thread_sched _ { make_bit_set (sys::cpu_info::instance ().PU_count ()).set () };

                for (PU_task & task : m_PU_tasks)
                {
                    task.m_thread = boost::thread { std::ref (* task.m_runnable) };
                }
            }

            seq.start_begin (); // note: matching 'start_end_wait()' is below
            {
                for (int32_t i = 0, turn_limit = m_dep_order.size (); i < turn_limit; ++ i, ++ m_last_started)
                {
                    component * const c = m_dep_order [i];

                    // if 'c' is 'startable' but not 'steppable', it is driven by the current container thread:

                    startable * const c_startable = dynamic_cast<startable *> (c);
                    if (c_startable)
                    {
                        mc::steppable * const c_steppable = dynamic_cast<mc::steppable *> (c);
                        if (! c_steppable)
                        {
                            std::string const & c_name = name_of (c);

                            LOG_trace2 << "\t(container thread) starting " << print (c_name) << ' ' << instance_name (c) << " ...";
                            seq.start (* c_startable);
                            LOG_trace2 << "\t(container thread) started " << print (c_name) << ' ' << instance_name (c);
                        }
                    }
                }
            }
            seq.start_end_wait (); // unblock all 'steppable's that have started (whether with start failures or not)
        }

        // report startup exceptions, if any, by throwing here:
        {
            boost::unique_lock<boost::mutex> _ { m_failure_mutex };

            if (m_start_failure != nullptr) std::rethrow_exception (m_start_failure); // note: non-null guard is required
        }
    }


    bool run_for (timestamp_t const duration)
    {
        return m_run_waitable.wait_for (duration);
    }

    bool run_until (timestamp_t const time_utc)
    {
        return m_run_waitable.wait_until (time_utc);
    }


    VR_ASSUME_COLD void stop ()
    {
        if (m_PU_tasks.empty ()) // TODO rm this branch once the below is solid for all situations
        {
            for (int32_t i = m_last_started; -- i >= 0; ) // note: stop in inverse order of starting
            {
                component * const c = m_dep_order [i];
                startable * const c_startable = dynamic_cast<startable *> (c);

                std::string const & c_name = name_of (c);

                if (c_startable)
                {
                    try
                    {
                        LOG_trace2 << "\tstopping " << print (c_name) << ' ' << instance_name (c) << " ...";
                        c_startable->stop ();
                        LOG_trace2 << "\tstopped " << print (c_name) << ' ' << instance_name (c);
                    }
                    catch (std::exception const & e)
                    {
                        LOG_error << "component " << print (c_name) << " FAILED to stop: " << exc_info (e);
                        break;
                    }
                    catch (...)
                    {
                        LOG_error << "component " << print (c_name) << " FAILED to stop: unknown exception";
                        break;
                    }
                }
            }
        }
        else
        {
            request_stop (); // break the composite step loops if not done by at least one of the steps:

            assert_nonnull (m_step_ctl);
            container_barrier & seq = m_step_ctl->m_sequencer;

            seq.run_end_wait (); // wait for all 'steppable' to decide to stop

            seq.stop_begin (); // reverse the latch sense
            {
                for (int32_t i = m_last_started; -- i >= 0; ) // note: stop in inverse order of starting
                {
                    component * const c = m_dep_order [i];

                    // if 'c' is 'startable' but not 'steppable', it is driven by the current container thread:

                    startable * const c_startable = dynamic_cast<startable *> (c);
                    if (c_startable)
                    {
                        mc::steppable * const c_steppable = dynamic_cast<mc::steppable *> (c);
                        if (! c_steppable)
                        {
                            std::string const & c_name = name_of (c);

                            LOG_trace2 << "\t(container thread) stopping " << print (c_name) << ' ' << instance_name (c) << " ...";
                            seq.stop (* c_startable);
                            LOG_trace2 << "\t(container thread) stopped " << print (c_name) << ' ' << instance_name (c);
                        }
                    }
                }
            }
            seq.stop_end (); // TODO this is redundant given the join next

            // not much task work is left after the stop barrier, but be tidy wrt TLS data cleanup:

            for (PU_task & task : m_PU_tasks)
            {
                if (task.m_thread.joinable ())
                    task.m_thread.join ();
            }
        }

        // report 'steppable' exceptions, if any, by throwing at least one of them here:
        {
            std::vector<std::exception_ptr> step_failures { }; // don't hold 'm_failure_mutex' long, make a copy of the list
            {
                boost::unique_lock<boost::mutex> _ { m_failure_mutex };

                step_failures = std::move (m_step_failures);
            }

            int32_t const x_limit = step_failures.size ();
            if (x_limit)
            {
                LOG_error << "********* " << x_limit <<  " component step FAILURE(s):";

                for (int32_t x = 0; x < x_limit; ++ x) // HACK "combine" all reported exception info
                {
                    std::exception_ptr const & eptr = step_failures [x];
                    assert_condition (eptr != nullptr);

                    if (x == (x_limit - 1)) // throw last one
                        std::rethrow_exception (eptr); // note: non-null guard is required
                    else // log the trace
                    {
                        try
                        {
                            std::rethrow_exception (eptr);
                        }
                        catch (std::exception const & e)
                        {
                            LOG_error << '[' << (x + 1) << '/' << x_limit << "] " << exc_info (e); // TODO or LOG_error ? msg looks messy, duplicated
                        }
                    }
                }
            }
        }
    }

    /*
     * DFS traversal starting at 'c'
     *
     * TODO guard against cycles
     */
    VR_ASSUME_COLD void bind_visit (component * const c, std::string const & c_name, std::vector<component *> & order, boost::unordered_set<component *> & visited)
    {
        check_nonnull (c->m_resolve_context);
        visited.insert (c);

        for (auto const & kv : c->m_resolve_context->m_dep_map)
        {
            std::string const & succ_name = std::get<0> (kv.second);

            auto const succ_i = m_components.left.find (succ_name);
            if (VR_UNLIKELY (succ_i == m_components.left.end ()))
                throw_x (invalid_input, "component " + print (c_name) + " references " + print (succ_name) + " which is not present");

            component * const succ = succ_i->second;

            // bind 'c' dep field to 'succ':

            component::bind_closure const & bind = std::get<1> (kv.second);
            bind (kv.first, succ);

            if (! visited.count (succ))
                bind_visit (succ, succ_name, order, visited); // DFS recursion
        }
        c->m_resolve_context.reset (); // won't need this anymore

        order.push_back (c);
    }

    std::string const & name_of (component * const c) const
    {
        assert_nonnull (c);

        auto const i = m_components.right.find (c);
        assert_positive (i != m_components.right.end ());

        return i->second;
    }

    /*
     * note: this can be invoked from threads other than the container's start()/stop() thread
     */
    void set_start_failure (std::exception_ptr const & eptr) VR_NOEXCEPT // TODO collect all start failures, since this one may be an effect, not the cause
    {
        {
            boost::unique_lock<boost::mutex> _ { m_failure_mutex };

            if (! m_start_failure) // idempotent
            {
                m_start_failure = eptr;
            }
        }
    }

    void add_step_failure (std::exception_ptr const & eptr) VR_NOEXCEPT
    {
        {
            boost::unique_lock<boost::mutex> _ { m_failure_mutex };

            // [caller ensures '(eptr != nullptr)']

            m_step_failures.push_back (eptr); // collect until stop/destruction time
        }
    }

    void add_stop_failure (std::exception_ptr const & eptr) VR_NOEXCEPT // see TODO in 'report_failure()'
    {
        try
        {
            std::rethrow_exception (eptr);
        }
        catch (std::exception const & e)
        {
            LOG_error << exc_info (e);
        }
    }

    void request_stop () VR_NOEXCEPT // TODO the externally-requested stop protocol needs more work
    {
        if (! m_run_waitable.is_signalled ()) // guard against spinlock
        {
            m_run_waitable.signal ();

            for (PU_task & task : m_PU_tasks) // broadcast
            {
                task.m_runnable->request_stop ();
            }
        }
    }


    std::vector<PU_task> m_PU_tasks { };
    PU_affinity_group_map m_PU_group_cfg { }; // parsed affinity configuration
    component_bimap m_components { }; // [non-owning] populated by 'configure()'
    std::vector<component *> m_dep_order { }; // [owning] dependencies at lower indices than dependent components
    std::unique_ptr<step_ctl_impl> m_step_ctl { };
    state::enum_t m_state { state::created };
    int32_t m_last_started { }; // tracks the number of successfully start()ed components [index into 'm_dep_order']
    mc::waitable m_run_waitable { };
    std::exception_ptr m_start_failure { }; // protected by 'm_failure_mutex'
    std::vector<std::exception_ptr> m_step_failures { }; // protected by 'm_failure_mutex'
    boost::mutex m_failure_mutex { };

}; // end of nested class
//............................................................................
//............................................................................

container::container (std::string const & name, settings const & cfg) :
    m_impl { std::make_unique<pimpl> (cfg) },
    m_name { (name.empty () ? instance_name (this) : name) }
{
}

container::~container () VR_NOEXCEPT
{
    if (m_impl)
    {
        try
        {
            stop ();
        }
        catch (std::exception const & e)
        {
            LOG_error << exc_info (e);
        }
        catch (...)
        {
            LOG_error << "container::stop() FAILED: unknown exception";
        }

        LOG_trace1 << quote_string<'\''> (m_name) << ": destructing ...";
        m_impl.reset ();    // VR_NOEXCEPT
        LOG_trace1 << quote_string<'\''> (m_name) << ": destructed";
    }
}
//............................................................................

container::configurator
container::configure ()
{
    return { * this };
}

container &
container::add (std::string const & name, component * const c)
{
    std::unique_ptr<component> c_ptr { c }; // grab ownership of 'c' before doing any checks

    pimpl & this_ = * m_impl;
    check_le (this_.m_state, state::configured);

    auto const rc = this_.m_components.insert (component_bimap::value_type { name, c });
    if (VR_UNLIKELY (! rc.second))
        throw_x (invalid_input, "duplicate component name (" + print (name) + ") or addr (" + hex_string_cast (c) + '}');

    LOG_trace1 << "added " << print (name) << ' ' << instance_name (c);
    c_ptr.release ();

    this_.m_state = state::configured;
    return (* this);
}
//............................................................................

bool
container::run_for (timestamp_t const duration)
{
    return m_impl->run_for (duration);
}

bool
container::run_until (timestamp_t const time_utc)
{
    return m_impl->run_until (time_utc);
}
//............................................................................

void
container::start ()
{
    pimpl & this_ = * m_impl;

    {
        LOG_trace1 << quote_string<'\''> (m_name) << ": resolving deps ...";

        if (this_.m_state != state::configured)
            throw_x (illegal_state, "container " + quote_string<'\''> (m_name) + " is not in a valid state to start: " + print (this_.m_state));

        this_.binding_traversal ();

        this_.m_state = state::resolved; // TODO eliminate this state?
    }
    {
        LOG_trace1 << quote_string<'\''> (m_name) << ": starting ...";

        if (this_.m_state != state::resolved)
            throw_x (illegal_state, "container " + quote_string<'\''> (m_name) + " is not in a valid state to start: " + print (this_.m_state));

        this_.start (* this); // note: this will advance the state to 'started' even in case of start() failures

        this_.m_state = state::started;
        LOG_trace1 << quote_string<'\''> (m_name) << ": started";
    }
}

void
container::stop ()
{
    pimpl & this_ = * m_impl;

    // [note: it is not an error never to 'start ()' a container (or it may have failed)]

    if (this_.m_state != state::stopped) // idempotency
    {
        this_.m_state = state::stopped; // must change the state because 'pimpl::stop()' may re-throw reported failures

        LOG_trace1 << quote_string<'\''> (m_name) << ": stopping ...";

        this_.stop ();

        LOG_trace1 << quote_string<'\''> (m_name) << ": stopped";
    }
}
//............................................................................

std::string const &
container::name () const
{
    return m_name;
}
//............................................................................

container::proxy
container::operator[] (std::string const & name) const
{
    auto const i = m_impl->m_components.left.find (name);
    if (VR_LIKELY (i != m_impl->m_components.left.end ()))
        return { * i->second };

    throw_x (invalid_input, quote_string<'\''> (m_name) + ": no component named " + print (name));
}
//............................................................................

std::string const &
container::name_of (component * const c) const
{
    pimpl & this_ = * m_impl;

    check_ge (this_.m_state, state::configured);

    return this_.name_of (c);
}
//............................................................................

void // TODO pass originating 'component' as well
container::report_failure (bool const in_start, std::exception_ptr const eptr) VR_NOEXCEPT
{
    pimpl & this_ = * m_impl;

    if (in_start)
        this_.set_start_failure (eptr);
    else
        this_.add_stop_failure (eptr);
}
//............................................................................

void
container::request_stop () VR_NOEXCEPT
{
    m_impl->request_stop ();
}

void
container::report_step_failure (std::exception_ptr const eptr) VR_NOEXCEPT
{
    if (eptr != nullptr) m_impl->add_step_failure (eptr); // note: "step" failure
}

} // end of 'di'
} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
