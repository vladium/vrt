
#include "vr/rt/tracing/tracing_ctl.h"

#include "vr/io/files.h"
#include "vr/io/uri.h"
#include "vr/settings.h"
#include "vr/sys/os.h"
#include "vr/utility.h"
#include "vr/util/datetime.h"
#include "vr/util/logging.h"
#include "vr/util/memory.h"
#include "vr/utility.h"
#include "vr/util/logging.h"

#include <boost/format.hpp>

#include <lttng/lttng.h>

//----------------------------------------------------------------------------
namespace vr
{
namespace rt
{
//............................................................................

static char
event_enabled_string (int32_t const value)
{
    switch (value)
    {
        case -1:    return ' ';
        case 0:     return '-';
        case 1:     return '*';

        default:    return '?';
    }
}

VR_EXPLICIT_ENUM (trace_event_type,
    int32_t,
        (LTTNG_EVENT_ALL,               all)
        (LTTNG_EVENT_TRACEPOINT,        tracepoint)
        (LTTNG_EVENT_PROBE,             probe)
        (LTTNG_EVENT_FUNCTION,          function)
        (LTTNG_EVENT_FUNCTION_ENTRY,    function_entry)
        (LTTNG_EVENT_NOOP,              noop)
        (LTTNG_EVENT_SYSCALL,           syscall)
    ,
    printable
);

VR_EXPLICIT_ENUM (trace_event_output,
    int32_t,
        (LTTNG_EVENT_SPLICE,    splice)
        (LTTNG_EVENT_MMAP,      mmap)
    ,
    printable
);

VR_EXPLICIT_ENUM (trace_event_loglevel,
    int32_t,
        (LTTNG_LOGLEVEL_EMERG,              emerg)
        (LTTNG_LOGLEVEL_ALERT,              alert)
        (LTTNG_LOGLEVEL_CRIT,               crit)
        (LTTNG_LOGLEVEL_ERR,                err)
        (LTTNG_LOGLEVEL_WARNING,            warning)
        (LTTNG_LOGLEVEL_NOTICE,             notice)
        (LTTNG_LOGLEVEL_INFO,               info)
        (LTTNG_LOGLEVEL_DEBUG_SYSTEM,       debug_system)
        (LTTNG_LOGLEVEL_DEBUG_PROGRAM,      debug_program)
        (LTTNG_LOGLEVEL_DEBUG_PROCESS,      debug_process)
        (LTTNG_LOGLEVEL_DEBUG_MODULE,       debug_module)
        (LTTNG_LOGLEVEL_DEBUG_UNIT,         debug_unit)
        (LTTNG_LOGLEVEL_DEBUG_FUNCTION,     debug_function)
        (LTTNG_LOGLEVEL_DEBUG_LINE,         debug_line)
        (LTTNG_LOGLEVEL_DEBUG,              debug)
        (-1,                                all)
    ,
    printable, parsable
);
//............................................................................

#define vr_CHECKED_LTTNG_CALL(exp) \
    ({ \
       decltype (exp) rc; rc = (exp); \
       if (VR_UNLIKELY (rc < 0)) \
           { throw_x (sys_exception, "(" #exp ") error (" + string_cast (rc) + "): " + ::lttng_strerror (rc)); }; \
       rc; \
    }) \
    /* */

#define vr_CHECKED_LTTNG_CALL_noexcept(exp) \
    ({ \
       decltype (exp) rc; rc = (exp); \
       if (VR_UNLIKELY (rc < 0)) \
           { LOG_error << "(" #exp ") error (" << rc << "): " << ::lttng_strerror (rc); }; \
       rc; \
    }) \
    /* */

//............................................................................

using handle_ptr        = std::unique_ptr<::lttng_handle, void (*) (::lttng_handle *)>;
using channel_ptr       = std::unique_ptr<::lttng_channel, void (*) (::lttng_channel *)>;

struct event_rule
{
    friend void from_json (json const & j, event_rule & obj)
    {
        check_condition (j.is_object (), j);

        obj.m_name = j.at ("name");

        if (j.count ("loglevel"))
        {
            std::string const & loglevel = j.at ("loglevel");
            check_nonempty (loglevel);

            obj.m_loglevel_is_single = (loglevel [0] == '=');
            obj.m_loglevel = to_enum<trace_event_loglevel> (loglevel.substr (obj.m_loglevel_is_single));
        }
    }

    std::string m_name { }; // can contain '*'s
    trace_event_loglevel::enum_t m_loglevel { trace_event_loglevel::all }; // default if "loglevel" not specified
    bool m_loglevel_is_single { false }; // [otherwise it's a range] default matching the above default

}; // end of class

//............................................................................
//............................................................................

struct tracing_ctl::state final
{
    state (settings const & cfg) :
        m_out_dir { io::date_specific_path (cfg.value<std::string> ("out_dir", "trace.%Y%m%d"), m_date) },
        m_subbuf_size { cfg.value<int64_t> ("subbuf.size", 512 * 1024L) },
        m_session_name { join_as_name (cfg.value<std::string> ("session", sys::proc_name ()), gd::to_iso_string (m_date)) },
        m_channel_name { cfg.value<std::string> ("channel", "C") },
        m_tracing_mandatory { cfg.value ("mandatory", true) }
    {
        for (auto const & er : cfg ["events"])
        {
            m_event_rules.emplace_back (er);
        }
    }


    util::date_t const m_date { util::current_date_local () }; // [note: need to preceed other fields] query once to avoid races
    stats m_loss_stats { };
    fs::path m_out_dir; // non-const because gets replaced with canonical/absolute final version
    int64_t const m_subbuf_size;
    std::string m_session_name; // non-const because may have to be adjusted to avoid a conflict
    std::string const m_channel_name;
    std::vector<event_rule> m_event_rules;
    ::lttng_domain m_trace_domain;
    handle_ptr m_trace_handle { nullptr, ::lttng_destroy_handle };
    bool const m_tracing_mandatory;
    bool m_tracing_started { false };

}; // end of nested class
//............................................................................
//............................................................................

tracing_ctl::tracing_ctl (settings const & cfg) :
    m_state { std::make_unique<state> (cfg) }
{
}

tracing_ctl::~tracing_ctl ()    = default; // pimpl
//............................................................................

void
tracing_ctl::start ()
{
    int32_t const trace_daemon_up = vr_CHECKED_LTTNG_CALL (::lttng_session_daemon_alive ());

    state & this_ = (* m_state);

    if (trace_daemon_up != 1)
    {
        if (this_.m_tracing_mandatory)
            throw_x (sys_exception, "tracing is mandatory, but lttng session daemon is not running");
        else
        {
            LOG_warn << "no tracing will be done: lttng session daemon is not running";
            return;
        }
    }

    fs::path const trace_out_path { io::weak_canonical_path (io::absolute_path (this_.m_out_dir)) };
    io::uri const trace_out_url { trace_out_path };

    // create an lttng session:
    {
        std::string session_name = this_.m_session_name; // note: copy

        for (int32_t retry = 1; true; ++ retry)
        {
            auto const rc = ::lttng_create_session (session_name.c_str (), trace_out_url.native ().c_str ());
            if (VR_LIKELY (rc >= 0))
                break;

            if (rc != - LTTNG_ERR_EXIST_SESS) // "Session name already exists"
                throw_x (sys_exception, "error (" + string_cast (rc) + "): " + ::lttng_strerror (rc));

            session_name = join_as_name<'_'> (this_.m_session_name, retry); // attempt to de-conflict the session name:
        }

        this_.m_session_name = session_name; // remember the name that succeeded
        this_.m_out_dir = trace_out_path; // remember the associated absoluate path
    }

    string_literal_t const session_name { this_.m_session_name.c_str () };
    VR_SCOPE_EXIT ([& this_, session_name]() { if (! this_.m_tracing_started) vr_CHECKED_LTTNG_CALL_noexcept (::lttng_destroy_session (session_name)); });

    // [at this point lttng cli tools would persist config in a file]

    LOG_trace1 << "created session " << print (session_name) << ", trace path " << print (trace_out_path);

    // set up 'm_trace_domain':
    {
        std::memset (& this_.m_trace_domain, 0, sizeof (this_.m_trace_domain)); // needed, as per docs

        this_.m_trace_domain.type = LTTNG_DOMAIN_UST;
        this_.m_trace_domain.buf_type = LTTNG_BUFFER_PER_PID; // note: changing buf types here results in different subbuf size defaults
    }

    this_.m_trace_handle.reset (::lttng_create_handle (session_name, & this_.m_trace_domain));
    if (VR_UNLIKELY (! this_.m_trace_handle)) throw_x (sys_exception, "failed to acquire an lttng handle");

    // create a custom channel instance (this just allocates the struct):

    channel_ptr const channel { ::lttng_channel_create (& this_.m_trace_domain), ::lttng_channel_destroy };
    {
        std::strncpy (channel->name, this_.m_channel_name.c_str (), LTTNG_SYMBOL_NAME_LEN);
        channel->name [LTTNG_SYMBOL_NAME_LEN - 1] = '\0';

        channel->attr.overwrite = 0; // [default] 'discard'
        channel->attr.output = LTTNG_EVENT_MMAP; // [default]
        channel->attr.subbuf_size = this_.m_subbuf_size; // note: default setting for LTTNG_BUFFER_PER_PID mode is 16k and will cause discards at extreme event rates
    }

    // ask the daemon to create this channel in the session:

    vr_CHECKED_LTTNG_CALL (::lttng_enable_channel (this_.m_trace_handle.get (), channel.get ()));

    // list available userspace tracepoints:

    if (LOG_trace_enabled (1))
    {
        ::lttng_event * trace_points { };
        int32_t const trace_point_count = vr_CHECKED_LTTNG_CALL(::lttng_list_tracepoints (this_.m_trace_handle.get (), & trace_points));
        VR_SCOPE_EXIT ([trace_points]() { std::free (trace_points); });

        {
            LOG_trace1 << "daemon is aware of " << trace_point_count << " UST tracepoint(s)";

            std::stringstream ss { }; ss.put ('\n');
            for (int32_t i = 0; i < trace_point_count; ++ i)
            {
                ::lttng_event const & e = trace_points [i];

                std::string const e_name { e.name, ::strnlen (e.name, LTTNG_SYMBOL_NAME_LEN) };

                ss << boost::format { "  %|-40s|: {%s, %s, %d}\n" }
                    % e_name
                    % static_cast<trace_event_type::enum_t> (e.type)
                    % static_cast<trace_event_loglevel::enum_t> (e.loglevel) // TODO loglevel_type
                    % e.pid;
            }

            LOG_trace1 << ss.str ();
        }
    }

    // enable requested event(s):
    {
        ::lttng_event ev; // this will be re-used for all requested events
        {
            std::memset (& ev, 0, sizeof (ev)); // needed, as per docs

            ev.type = LTTNG_EVENT_TRACEPOINT;
        }

        for (event_rule const & er : this_.m_event_rules)
        {
            std::strncpy (ev.name, er.m_name.c_str (), LTTNG_SYMBOL_NAME_LEN);
            ev.name [LTTNG_SYMBOL_NAME_LEN - 1] = '\0';

            ev.loglevel_type = (er.m_loglevel_is_single ? LTTNG_EVENT_LOGLEVEL_SINGLE : LTTNG_EVENT_LOGLEVEL_RANGE);
            ev.loglevel = er.m_loglevel;

            vr_CHECKED_LTTNG_CALL (::lttng_enable_event (this_.m_trace_handle.get (), & ev, this_.m_channel_name.c_str ()));
        }
    }

    // TODO consider adding context with 'ip' for later source file/line mapping (although those could be passed via args also)


    // list events (TODO confirm the list contains what we've enabled ?):

    if (LOG_trace_enabled (1))
    {
        ::lttng_event * trace_events { };
        int32_t const trace_event_count = vr_CHECKED_LTTNG_CALL (::lttng_list_events (this_.m_trace_handle.get (), this_.m_channel_name.c_str (), & trace_events));
        VR_SCOPE_EXIT ([trace_events]() { std::free (trace_events); });

        if (trace_event_count > 0)
        {
            LOG_trace1 << "daemon is aware of " << trace_event_count << " UST event(s)";

            std::stringstream ss { }; ss.put ('\n');

            for (int32_t i = 0; i < trace_event_count; ++ i)
            {
                ::lttng_event const & e = trace_events [i];

                std::string const e_name { e.name, ::strnlen (e.name, LTTNG_SYMBOL_NAME_LEN) };

                ss << boost::format { "  [%|1c|] %|-36s|: {%s, %s}\n" }
                    % event_enabled_string (e.enabled)
                    % e_name
                    % static_cast<trace_event_type::enum_t> (e.type)
                    % static_cast<trace_event_loglevel::enum_t> (e.loglevel); // TODO loglevel_type
            }

            LOG_trace1 << ss.str ();
        }
    }

    // list session channels to confirm that ours is present, enabled, is in requested 'overwrite' and 'output' modes, and has subbufs of no smaller than the requested size:
    {
        ::lttng_channel * trace_channels { };
        int32_t trace_channel_count = vr_CHECKED_LTTNG_CALL (::lttng_list_channels (this_.m_trace_handle.get (), & trace_channels));
        VR_SCOPE_EXIT ([trace_channels]() { std::free (trace_channels); });

        check_positive (trace_channel_count); // must contain at least ours
        bool channel_registered { false };

        for (int32_t i = 0; i < trace_channel_count; ++ i)
        {
            ::lttng_channel const & c = trace_channels [i];

            std::string const c_name { c.name, ::strnlen (c.name, LTTNG_SYMBOL_NAME_LEN) };

            if (c_name == this_.m_channel_name) // match found
            {
                check_eq (c.enabled, true);
                check_eq (c.attr.overwrite, channel->attr.overwrite);
                check_eq (c.attr.output, channel->attr.output);
                check_ge (c.attr.subbuf_size, channel->attr.subbuf_size);

                channel_registered = true;
                break;
            }
        }

        check_positive (channel_registered);
    }

    // start tracing:

    LOG_trace1 << "starting trace session " << print (session_name) << " ...";
    vr_CHECKED_LTTNG_CALL (::lttng_start_tracing (session_name));

    this_.m_tracing_started = true; // commit
    {
        auto const session_info = session ();
        LOG_info << "started tracing session " << print (std::get<0> (session_info)) << "\n  trace out dir " << print (std::get<1> (session_info));
    }
}

void
tracing_ctl::stop ()
{
    assert_condition (m_state);
    state & this_ = (* m_state);

    if (this_.m_tracing_started)
    {
        // TODO handle the case where sessiond may have crashed? the lttng API calls below will fail in that case anyway

        string_literal_t const session_name { this_.m_session_name.c_str () };

        // stop tracing:

        LOG_info << "flushing trace session " << print (session_name) << "...";
        vr_CHECKED_LTTNG_CALL_noexcept (::lttng_stop_tracing (session_name));

        bool channel_found { false };

        // list session channels to find our domain by name:
        {
            assert_nonnull (this_.m_trace_handle); // created in 'start ()'

            ::lttng_channel * trace_channels { };
            int32_t trace_channel_count = vr_CHECKED_LTTNG_CALL (::lttng_list_channels (this_.m_trace_handle.get (), & trace_channels));
            VR_SCOPE_EXIT ([trace_channels]() { std::free (trace_channels); });

            check_positive (trace_channel_count); // must contain at least ours

            for (int32_t i = 0; i < trace_channel_count; ++ i)
            {
                ::lttng_channel & c = trace_channels [i];

                std::string const c_name { c.name, ::strnlen (c.name, LTTNG_SYMBOL_NAME_LEN) };

                if (c_name == this_.m_channel_name) // match found
                {
                    stats & lst { this_.m_loss_stats };
                    {
                        vr_CHECKED_LTTNG_CALL_noexcept (::lttng_channel_get_discarded_event_count (& c, & unsigned_cast (lst.m_discarded_events)));
                        vr_CHECKED_LTTNG_CALL_noexcept (::lttng_channel_get_lost_packet_count (& c, & unsigned_cast (lst.m_lost_packets)));
                    }

                    channel_found = true;
                    break;
                }
            }
        }

        this_.m_trace_handle.reset (); // won't need it again

        // destroy session (this does not destroy trace output data):

        vr_CHECKED_LTTNG_CALL (::lttng_destroy_session (session_name));
        LOG_info << "closed trace session " << print (session_name);

        if (channel_found)
        {
            stats const & lst { this_.m_loss_stats };

            if (lst.m_discarded_events || lst.m_lost_packets)
            {
                LOG_warn << "***** discarded " << lst.m_discarded_events << " trace event(s), dropped: " << lst.m_lost_packets << " trace packet(s) *****";
            }
        }
    }
}
//............................................................................

std::tuple<std::string, fs::path>
tracing_ctl::session () const
{
    check_condition (m_state->m_tracing_started);

    return { m_state->m_session_name, m_state->m_out_dir };
}
//............................................................................

tracing_ctl::stats const &
tracing_ctl::loss_stats () const
{
    return m_state->m_loss_stats;
}

} // end of 'rt'
} // end of namespace
//----------------------------------------------------------------------------
