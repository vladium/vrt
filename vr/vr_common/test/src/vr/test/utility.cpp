
#include "vr/test/utility.h"

#include "vr/io/files.h"
#include "vr/strings.h"
#include "vr/sys/os.h"
#include "vr/util/env.h"
#include "vr/util/singleton.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace test
{
//............................................................................
//............................................................................

uint64_t env::s_random_seed { };

//............................................................................

void
env::SetUp ()
{
    uint64_t s = util::getenv<uint64_t> (VR_ENV_TEST_SEED, 0);
    if (s)
        LOG_warn << "[found " VR_ENV_TEST_SEED " override]";
    else
    {
        s = (VR_TSC_START () | 1);
        s = util::xorshift (s);
    }

    s_random_seed = s;
    LOG_info << "random seed: " << s_random_seed;

    // unless disabled, clean temp dir used by testcases:

    fs::path const test_temp_dir = test::temp_dir ();

    if ("no" != util::getenv (VR_ENV_TEST_TEMP_CLEAN))
    {
        LOG_trace1 << "cleaning [" << test_temp_dir.native () << "] ...";

        io::clean_dir (test_temp_dir, false);
    }
    else
    {
        LOG_warn << "retaining contents of [" << test_temp_dir.native () << "] ...";
    }
}

void
env::TearDown ()
{
    LOG_info << "random seed: " << s_random_seed;
}
//............................................................................
//............................................................................
namespace
{

struct temp_dir_factory
{
    temp_dir_factory (fs::path * const obj) :
        m_obj { obj }
    {
        std::string const name { join_as_name (VR_APP_NAME, util::getenv<std::string> ("USER")) };
        new (obj) fs::path { io::temp_dir () / name VR_IF_RELEASE (/ "release") };

        LOG_info << "test temp dir: " << print (* obj);
    }

    ~temp_dir_factory ()
    {
        if (m_obj) util::destruct (* m_obj);
    }

    fs::path * const m_obj;

}; // end of class

struct data_dir_factory
{
    data_dir_factory (fs::path * const obj) :
        m_obj { obj }
    {
        // unless overridden, use a path relative to the test binary (this works in both work and team dev envs):

        new (obj) fs::path { util::getenv<fs::path> (VR_ENV_TEST_DATA_ROOT,
                             io::absolute_path (sys::proc_name (false)).parent_path () / "../test/data") };

        LOG_info << "test data dir: " << print (* obj);

        check_condition (! obj->empty (), VR_ENV_TEST_DATA_ROOT);
        check_condition (fs::exists (* obj), VR_ENV_TEST_DATA_ROOT);

        (* obj) = io::absolute_path (* obj);
    }

    ~data_dir_factory ()
    {
        if (m_obj) util::destruct (* m_obj);
    }

    fs::path * const m_obj;

}; // end of class

struct mcast_ifc_factory
{
    mcast_ifc_factory (std::string * const obj) :
        m_obj { obj }
    {
        new (obj) std::string { util::getenv<std::string> (VR_ENV_TEST_NET_IFC, "lo") };

        LOG_info << "test mcast ifc: " << print (* obj);
    }

    ~mcast_ifc_factory ()
    {
        if (m_obj) util::destruct (* m_obj);
    }

    std::string * const m_obj;

}; // end of class

} // end of anonymous
//............................................................................
//............................................................................

std::string
current_test_name ()
{
    auto const * const ti = gt::UnitTest::GetInstance ()->current_test_info ();

    return (ti ? join_as_name (ti->test_case_name (), ti->name ()) : "");
}

fs::path
temp_dir ()
{
    return util::singleton<fs::path, temp_dir_factory>::instance ();
}

fs::path const &
data_dir ()
{
    return util::singleton<fs::path, data_dir_factory>::instance ();
}

fs::path
unique_test_path (std::string const & name_suffix)
{
    return io::unique_path (temp_dir () / current_test_name (), name_suffix);
}
//............................................................................

std::string const &
mcast_ifc ()
{
    return util::singleton<std::string, mcast_ifc_factory>::instance ();
}
//............................................................................

int32_t
read_lines (std::istream & in, string_vector & out)
{
    int32_t rc { };

    for (std::string line; std::getline (in, line); ++ rc)
    {
        out.emplace_back (std::move (line));
    }

    return rc;
}

} // end of 'test'
} // end of namespace
//----------------------------------------------------------------------------
