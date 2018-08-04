
#include "vr/rt/cfg/resources.h"

#include "vr/io/files.h"
#include "vr/sys/os.h"
#include "vr/util/env.h"
#include "vr/util/logging.h"
#include "vr/util/singleton.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace rt
{
//............................................................................
//............................................................................
namespace
{

struct data_root_factory final
{
    data_root_factory (fs::path * const obj) :
        m_obj { obj }
    {
        new (obj) fs::path { util::getenv<fs::path> (VR_ENV_DATA_ROOT, io::absolute_path (sys::proc_name (false)).parent_path () / "../data") };

        if (! obj->empty ())
        {
            (* obj) = io::absolute_path (* obj);

            LOG_trace1 << "data root " << print (* obj);
        }
    }

    ~data_root_factory ()
    {
        if (m_obj) util::destruct (* m_obj);
    }

    fs::path * const m_obj;

}; // end of class

} // end of anonymous
//............................................................................
//............................................................................

io::uri
resolve_as_uri (std::string const & path_or_name)
{
    LOG_trace1 << "resolving " << print (path_or_name) << ": cwd " << print (fs::current_path ()) << ", cfg root " << print (util::singleton<fs::path, data_root_factory>::instance ());

    // (1) see if the input is already in a URL form:

    try
    {
        return { path_or_name };
    }
    catch (invalid_input const & ii)
    {
        // [continue]
    }

    fs::path const & cfg_root { util::singleton<fs::path, data_root_factory>::instance () };

    fs::file_status const st = fs::status (cfg_root);
    bool const cfg_root_usable = (fs::exists (st) && fs::is_directory (st));

    if (! cfg_root_usable)
        LOG_warn << "data root " << print (cfg_root) << " doesn't exist: ignoring";

    // (2) if the string is actually empty, return a hardcoded default relative to 'cfg_root':

    if (path_or_name.empty () && cfg_root_usable)
        return { cfg_root / "app_cfg.json" };


    fs::path const pn { path_or_name };

    // (3) check if 'pn' is a valid (existing) filename relative to the current path:

    fs::path const cwd = fs::current_path ();

    if (io::path_valid (pn, cwd)) // note: an absolute path 'pn' will ignore 'cwd'
    {
        fs::path const p { io::absolute_path (pn, cwd) };
        if (! fs::is_directory (p))
            return { p } ;
    }

    if (cfg_root_usable)
    {
        // (4) check if 'pn' is a valid (existing) filename relative to 'cfg_root':

        if (io::path_valid (pn, cfg_root)) // note: an absolute path 'pn' will ignore 'cfg_root'
        {
            fs::path const p { io::absolute_path (pn, cfg_root) };
            if (! fs::is_directory (p))
                return { p } ;
        }

        // (5) otherwise, search for 'pn' as a subpath somewhere below 'cfg_root':

        optional<fs::path> const pmatch = io::find_path_recursive (pn, cfg_root);
        if (pmatch)
            return (* pmatch);
    }

    throw_x (invalid_input, "could not resolve " + print (path_or_name) + " either as a [relative] path or a url (cwd: " + print (cwd) + ", cfg root: " + print (cfg_root) + ')');
}

} // end of 'rt'
} // end of namespace
//----------------------------------------------------------------------------
