
#include "vr/rt/cfg/app_cfg.h"

#include "vr/io/files.h"
#include "vr/settings.h"
#include "vr/util/logging.h"
#include "vr/util/memory.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace rt
{
//............................................................................

static const scope_path g_seed_path { "/app_cfg/seed" };
static const scope_path g_time_path { "/app_cfg/time" };

//............................................................................

struct app_cfg::pimpl final
{
    pimpl (io::uri const & source)
    {
        LOG_trace1 << "loading cfg from " << print (source) << " ...";

        // assume JSON content (for now, see ASX-48):

        m_root = io::read_json (source);
        initialize ();
    }

    pimpl (settings const & cfg) :
        m_root (cfg) // deep copy
    {
        initialize ();
    }

    pimpl (settings && cfg) :
        m_root (std::move (cfg))
    {
        initialize ();
    }

    void initialize ();


    VR_FORCEINLINE bool scope_exists (scope_path const & path) const
    {
        try // TODO is there a more efficient way?
        {
            m_root.at (json::json_pointer { path });
            return true;
        }
        catch (std::exception const & e)
        {
            return false;
        }
    }

    settings const & scope (scope_path const & path) const
    {
        LOG_trace2 << "looking up scope " << print (path);
        try
        {
            return m_root.at (json::json_pointer { path });
        }
        catch (std::exception const & e)
        {
            chain_x (invalid_input, "failed to look up scope path " + print (path));
        }

        VR_ASSUME_UNREACHABLE (); // suppress IDE warning
    }


    util::ptime_t m_start_time { };
    uint64_t m_rng_seed { };
    settings m_root { };

}; // end of nested class
//............................................................................

void
app_cfg::pimpl::initialize ()
{
    m_start_time = scope_exists (g_time_path)
        ? util::parse_ptime (scope (g_time_path).get<std::string> ())
        : util::current_time_local ();

    m_rng_seed = scope_exists (g_seed_path)
        ? scope (g_seed_path).get<uint64_t> ()
        : (m_start_time.date ().day_of_year () | m_start_time.time_of_day ().total_microseconds ());

    LOG_trace1 << "[session: " << m_start_time << "] loaded cfg " << print (m_root.type ()) << " with " << m_root.size () << " root children";
    LOG_trace2 << '\n' << print (m_root);
}
//............................................................................

app_cfg::app_cfg (io::uri const & source) :
    m_impl { std::make_unique<pimpl> (source) }
{
}

app_cfg::app_cfg (settings const & cfg) :
    m_impl { std::make_unique<pimpl> (cfg) }
{
}

app_cfg::app_cfg (settings && cfg) :
    m_impl { std::make_unique<pimpl> (std::forward<settings> (cfg)) }
{
}

app_cfg::~app_cfg ()    = default; // pimpl
//............................................................................

settings const &
app_cfg::root () const
{
    return m_impl->m_root;
}

bool
app_cfg::scope_exists (scope_path const & path) const
{
    return m_impl->scope_exists (path);
}

settings const &
app_cfg::scope (scope_path const & path) const
{
    return m_impl->scope (path);
}
//............................................................................

util::date_t
app_cfg::start_date () const
{
    return m_impl->m_start_time.date ();
}

util::ptime_t const &
app_cfg::start_time () const
{
    return m_impl->m_start_time;
}
//............................................................................

uint64_t const &
app_cfg::rng_seed () const
{
    return m_impl->m_rng_seed;
}

} // end of 'rt'
} // end of namespace
//----------------------------------------------------------------------------
