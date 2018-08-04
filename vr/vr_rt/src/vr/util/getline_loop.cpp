
#include "vr/util/getline_loop.h"

#include "vr/filesystem.h"
#include "vr/util/env.h"
#include "vr/util/singleton.h"
#include "vr/sys/os.h"

#include <boost/algorithm/string/trim.hpp>

VR_DIAGNOSTIC_PUSH ()
VR_DIAGNOSTIC_IGNORE ("-Wsign-compare")
#   include "vr/util/impl/linenoise.hpp"
VR_DIAGNOSTIC_POP ()

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
//............................................................................
//............................................................................
namespace
{

#if !defined (VR_ENV_CMD_HISTORY)
#   define VR_ENV_CMD_HISTORY       VR_APP_NAME "_CMD_HISTORY"
#endif

struct linenoise_setup
{
    linenoise_setup () :
        m_history_file { util::getenv<fs::path> (VR_ENV_CMD_HISTORY, "." + sys::proc_name () + "_history") }
    {
        ::linenoise::SetMultiLine (true);
        ::linenoise::SetHistoryMaxLen (64);

        ::linenoise::LoadHistory (m_history_file.c_str ());
    }

    ~linenoise_setup ()
    {
        ::linenoise::SaveHistory (m_history_file.c_str ());
    }

    void history_add (string_literal_t const s)
    {
        ::linenoise::AddHistory (s);
    }

    fs::path const m_history_file;

}; // end of class

} // end of anonymous
//............................................................................
//............................................................................

getline_loop::getline_loop (std::string const & prompt) :
    m_prompt { prompt }
{
}

getline_loop::getline_loop (string_literal_t const prompt) :
    m_prompt { prompt }
{
}
//............................................................................

bool
getline_loop::next (std::string & /* out */line)
{
    linenoise_setup & lns = util::singleton<linenoise_setup>::instance ();

    line.clear ();

    ::linenoise::Readline (m_prompt.c_str (), line);

    boost::trim (line); // in-place

    bool const r = (! line.empty ());
    if (r) lns.history_add (line.c_str ());

    return r;
}

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
