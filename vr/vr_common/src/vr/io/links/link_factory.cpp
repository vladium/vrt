
#include "vr/io/links/link_factory.h"

#include "vr/io/files.h"
#include "vr/util/env.h"
#include "vr/util/logging.h"
#include "vr/util/singleton.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
//............................................................................
//............................................................................
namespace
{

struct link_cap_root_factory final
{
    link_cap_root_factory (fs::path * const obj) :
        m_obj { obj }
    {
        new (obj) fs::path { util::getenv<fs::path> (VR_ENV_LINK_CAP_ROOT, "/dev/shm/link.capture") };

        if (! obj->empty ())
        {
            (* obj) = io::absolute_path (* obj);

            LOG_trace1 << "link capture root " << print (* obj);
        }
    }

    ~link_cap_root_factory ()
    {
        if (m_obj) util::destruct (* m_obj);
    }

    fs::path * const m_obj;

}; // end of class

} // end of anonymous
//............................................................................
//............................................................................

fs::path
create_link_capture_path (std::string const & name_prefix, fs::path const & root, util::ptime_t const & stamp)
{
    check_nonempty (name_prefix);
    check_nonempty (root);
    check_condition (! stamp.is_special ());

    std::string const suffix = util::format_time (stamp, "%Y%m%d.%H%M%S");

    // date/time-specific capture dir:

    fs::path const cap_dir { root / suffix };
    io::create_dirs (cap_dir);

    return { cap_dir / name_prefix };
}

fs::path
create_link_capture_path (std::string const & name_prefix, arg_map const & opt_args)
{
    fs::path const root = opt_args.get<fs::path> ("root", util::singleton<fs::path, link_cap_root_factory>::instance ());
    util::ptime_t stamp = ( opt_args.count ("stamp") ? opt_args.get<util::ptime_t> ("stamp") : util::current_time_local ());

    return create_link_capture_path (name_prefix, root, stamp);
}

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
