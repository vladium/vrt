
#include "vr/test/files.h"

#include "vr/util/env.h"
#include "vr/util/logging.h"
#include "vr/util/singleton.h"

#include "vr/test/utility.h"

#include <set>

//----------------------------------------------------------------------------
namespace vr
{
namespace test
{
//............................................................................
//............................................................................
namespace
{

struct cap_DAO_ASX final
{
    using date_set      = std::set<util::date_t>;

    cap_DAO_ASX () :
        m_root { util::getenv<fs::path> (VR_ENV_CAP_ROOT) / string_cast (market::source::ASX) }
    {
        LOG_trace1 << "using capture root " << print (m_root);

        try
        {
            for (fs::directory_entry const & e : fs::directory_iterator { m_root })
            {
                if (fs::is_directory (e.status ()))
                {
                    try // to parse as date
                    {
                        m_dates.insert (util::parse_date (e.path ().filename ().native ()));
                    }
                    catch (invalid_input const & ii)
                    {
                        continue;
                    }
                }
            }
        }
        catch (fs::filesystem_error const & fe)
        {
            LOG_error << "could not read " << print (m_root) << ": " << fe.what ();
        }

        LOG_trace1 << "found " << m_dates.size () << " capture date(s)";
    }


    fs::path complete_path (util::date_t const & date) const
    {
        assert_condition (! date.is_special ()); // caller ensures

        return m_root / gd::to_iso_string (date) / "asx-02";
    }


    fs::path query (rop::enum_t const op, util::date_t const & date) const
    {
        date_set::const_iterator i { };

        switch (op)
        {
            case "<"_rop:
            {
                i = m_dates.lower_bound (date);
                if (i != m_dates.begin ()) -- i;
            }
            break;

            case "<="_rop:
            {
                i = m_dates.upper_bound (date);
                if (i != m_dates.begin ()) -- i;
            }
            break;

            case "=="_rop:
            {
                i = m_dates.find (date);
            }
            break;

            case ">="_rop:
            {
                i = m_dates.lower_bound (date);
            }
            break;

            case ">"_rop:
            {
                i = m_dates.upper_bound (date);
            }
            break;

            default: VR_ASSUME_UNREACHABLE ();

        } // end of switch

        if (i != m_dates.end ())
            return complete_path (* i);

        throw_x (invalid_input, "no capture data found " + print (op) + ' ' + print (date));
    }


    fs::path const m_root;
    date_set m_dates { };

}; // end of class


} // end of anonymous
//............................................................................
//............................................................................

fs::path
find_capture (market::source::enum_t const src, rop::enum_t const op, util::date_t const & date)
{
    check_ne (op, rop::NE); // unsupported query type

    return util::singleton<cap_DAO_ASX>::instance ().query (op, date);
}

} // end of 'test'
} // end of namespace
//----------------------------------------------------------------------------
