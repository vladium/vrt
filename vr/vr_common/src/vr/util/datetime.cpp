
#include "vr/util/datetime.h"

#include "vr/filesystem.h"
#include "vr/io/streams.h"
#include "vr/sys/os.h"
#include "vr/util/format.h"
#include "vr/util/logging.h"
#include "vr/util/parse.h"
#include "vr/util/singleton.h"

#include <boost/io/ios_state.hpp>
#if VR_USE_BOOST_LOCALE
#   include <boost/locale.hpp>
#endif // VR_USE_BOOST_LOCALE
#include <boost/regex.hpp>

#include <locale>

#include <unicode/gregocal.h>
#include <unicode/timezone.h>

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
//............................................................................
//............................................................................
namespace
{

#define vr_ptime_format_sec     "%Y-%b-%d %H:%M:%S"

std::locale const g_ptime_in_loc  { std::locale::classic (), new pt::time_input_facet { vr_ptime_format_sec } }; // note: locale obj takes facet ownership
std::locale const g_ptime_out_loc { std::locale::classic (), new pt::time_facet { vr_ptime_format_sec } }; // note: locale obj takes facet ownership

} // end of anonymous
//............................................................................
//............................................................................

date_t const &
epoch_date ()
{
    static date_t const g_epoch { 1970, gd::Jan, 1 };

    return g_epoch;
}

ptime_t const &
epoch_time ()
{
    static ptime_t const g_epoch { epoch_date (), pt::seconds { 0 } };

    return g_epoch;
}
//............................................................................

void
print_timestamp_as_ptime (std::ostream & os, timestamp_t const ts_utc, int32_t const digits_secs)
{
    boost::io::basic_ios_locale_saver<std::ostream::char_type, std::ostream::traits_type> _ { os };
    {
        os.imbue (g_ptime_out_loc);
    }

    os << ptime_t { g_epoch_date, pt::seconds { ts_utc / 1000000000 } };

    if (VR_LIKELY (digits_secs > 0))
    {
        char buf [10] = { '.', '0', '0', '0', '0', '0', '0', '0', '0', '0' };
        util::rjust_print_decimal_nonnegative (ts_utc % 1000000000, buf + 1, 9);

        os.write (buf, 1 + digits_secs);
    }
}
//............................................................................

void
format_date (call_traits<date_t>::param date, std::string const & format, std::ostream & os)
{
    std::locale const out_loc { std::locale::classic (), new gd::date_facet { format.c_str () } };  // note: locale obj takes facet ownership

    boost::io::basic_ios_locale_saver<std::ostream::char_type, std::ostream::traits_type> _ { os };
    {
        os.imbue (out_loc);
    }

    os << date;
}

void
format_time (call_traits<ptime_t>::param ptime, std::string const & format, std::ostream & os)
{
    std::locale const out_loc { std::locale::classic (), new pt::time_facet { format.c_str () } };  // note: locale obj takes facet ownership

    boost::io::basic_ios_locale_saver<std::ostream::char_type, std::ostream::traits_type> _ { os };
    {
        os.imbue (out_loc);
    }

    os << ptime;
}
//............................................................................

template<>
ptime_t
parse_ptime</* SAVE_IS_STATE */false> (std::istream & is)
{
    is.imbue (g_ptime_in_loc); // TODO is this idempotent or should I check

    ptime_t r;
    is >> r;

    assert_condition (! r.is_special ());

    return r;
}

template<>
ptime_t
parse_ptime</* SAVE_IS_STATE */true> (std::istream & is)
{
    boost::io::basic_ios_locale_saver<std::ostream::char_type, std::ostream::traits_type> _ { is };
    {
        is.imbue (g_ptime_in_loc);
    }

    ptime_t r;
    is >> r;

    assert_condition (! r.is_special ());

    return r;
}
//............................................................................

ptime_t
parse_ptime (char_const_ptr_t const start, int32_t const size)
{
    assert_nonnull (start);

    auto const dot = static_cast<char_const_ptr_t> (std::memchr (start, '.', size));

    if (dot != nullptr)
    {
        int32_t const sec_size = static_cast<int32_t> (dot - start);

        ptime_t pt_sec;
        {
            io::array_istream<> is { start, sec_size };

            pt_sec = parse_ptime<false> (is);
        }

        int32_t const digits_secs = size - sec_size - 1;

        timestamp_t subsecond = util::parse_decimal_nonnegative<timestamp_t> (dot + 1, digits_secs);
        assert_nonnegative (subsecond);

        switch (digits_secs)
        {
            case 1: subsecond *= 100000000; break;
            case 2: subsecond *= 10000000; break;
            case 3: subsecond *= 1000000; break;
            case 4: subsecond *= 100000; break;
            case 5: subsecond *= 10000; break;
            case 6: subsecond *= 1000; break;
            case 7: subsecond *= 100; break;
            case 8: subsecond *= 10; break;
            case 9: break;

            default: VR_ASSUME_UNREACHABLE ();

        } // end of switch

        return (pt_sec + subsecond_duration_ns { subsecond });
    }
    else
    {
        io::array_istream<> is { start, size };

        return parse_ptime<false> (is);
    }
}
//............................................................................

timestamp_t
parse_ptime_as_timestamp (char_const_ptr_t const start, int32_t const size)
{
    assert_nonnull (start);

    auto const dot = static_cast<char_const_ptr_t> (std::memchr (start, '.', size));

    ptime_t pt_sec;
    timestamp_t subsecond { };

    if (dot != nullptr)
    {
        int32_t const sec_size = static_cast<int32_t> (dot - start);

        {
            io::array_istream<> is { start, sec_size };

            pt_sec = parse_ptime<false> (is);
        }

        int32_t const digits_secs = size - sec_size - 1;

        subsecond = util::parse_decimal_nonnegative<timestamp_t> (dot + 1, digits_secs);
        assert_nonnegative (subsecond);

        switch (digits_secs)
        {
            case 1: subsecond *= 100000000; break;
            case 2: subsecond *= 10000000; break;
            case 3: subsecond *= 1000000; break;
            case 4: subsecond *= 100000; break;
            case 5: subsecond *= 10000; break;
            case 6: subsecond *= 1000; break;
            case 7: subsecond *= 100; break;
            case 8: subsecond *= 10; break;
            case 9: break;

            default: VR_ASSUME_UNREACHABLE ();

        } // end of switch
    }
    else
    {
        io::array_istream<> is { start, size };

        pt_sec = parse_ptime<false> (is);
    }

    auto const epoch_offset = pt_sec - g_epoch_time;

    return (epoch_offset.total_seconds () * _1_second () + subsecond);
}
//............................................................................

VR_FORCEINLINE int32_t
digit (char const c)
{
    assert_in_inclusive_range (static_cast<uint32_t> (c), '0', '9');

    return (c - '0');
}

timestamp_t
parse_HHMMSS (char_const_ptr_t const start, int32_t const digits_secs)
{
    int32_t const hrs   = (digit (start [0]) * 10 + digit (start [1]));
    int32_t const mins  = (digit (start [2]) * 10 + digit (start [3]));
    int32_t const secs  = (digit (start [4]) * 10 + digit (start [5]));

    timestamp_t r = (hrs * 3600 + mins * 60 + secs) * _1_second ();

    if (digits_secs > 0)
    {
        timestamp_t subsecond = util::parse_decimal_nonnegative<timestamp_t> (start + 6, digits_secs);

        switch (digits_secs)
        {
            case 1: subsecond *= 100000000; break;
            case 2: subsecond *= 10000000; break;
            case 3: subsecond *= 1000000; break;
            case 4: subsecond *= 100000; break;
            case 5: subsecond *= 10000; break;
            case 6: subsecond *= 1000; break;
            case 7: subsecond *= 100; break;
            case 8: subsecond *= 10; break;
            case 9: break;

            default: VR_ASSUME_UNREACHABLE ();

        } // end of switch

        r += subsecond;
    }

    return r;
}
//............................................................................

timestamp_t
parse_duration_as_timestamp (char_const_ptr_t const start, int32_t const size)
{
    assert_nonnull (start);

    auto const dot = static_cast<char_const_ptr_t> (std::memchr (start, '.', size));

    time_diff_t td_sec;
    timestamp_t subsecond { };

    if (dot != nullptr)
    {
        int32_t const sec_size = static_cast<int32_t> (dot - start);

        td_sec = pt::duration_from_string ({ start, static_cast<std::size_t> (sec_size) });

        int32_t const digits_secs = size - sec_size - 1;

        subsecond = util::parse_decimal_nonnegative<timestamp_t> (dot + 1, digits_secs);
        assert_nonnegative (subsecond);

        switch (digits_secs)
        {
            case 1: subsecond *= 100000000; break;
            case 2: subsecond *= 10000000; break;
            case 3: subsecond *= 1000000; break;
            case 4: subsecond *= 100000; break;
            case 5: subsecond *= 10000; break;
            case 6: subsecond *= 1000; break;
            case 7: subsecond *= 100; break;
            case 8: subsecond *= 10; break;
            case 9: break;

            default: VR_ASSUME_UNREACHABLE ();

        } // end of switch
    }
    else
    {
        td_sec = pt::duration_from_string ({ start, static_cast<std::size_t> (size) });
    }

    return (td_sec.total_seconds () * _1_second () + subsecond);
}
//............................................................................

date_t
extract_date (std::string const & s)
{
    boost::regex const g_re { R"(.*?(?'date'[1-2]\d{3}[0-1]\d[0-3]\d).*)" };

    boost::smatch m { };
    if (VR_LIKELY (boost::regex_match (s, m, g_re)) && (m.size () > 1))
        return gd::from_undelimited_string ({ m ["date"].first, m ["date"].second });
    else
        return date_t { gd::not_a_date_time };
}

extern date_t
parse_date (std::string const & s)
{
    try
    {
        return gd::from_string (s);
    }
    catch (boost::bad_lexical_cast const & blc)
    {
        try
        {
            return gd::from_undelimited_string (s);
        }
        catch (...)
        {
            throw_x (invalid_input, "failed to parse " + print (s) + "' as date");
        }
    }
}

std::tuple<date_t, date_t>
parse_date_range (std::string const & s)
{
    string_vector const tokens = util::split (s, "-, ");
    if (tokens.size () != 2)
        throw_x (invalid_input, "failed to parse " + print (s) + "' as date range");

    return std::make_tuple (parse_date (tokens [0]), parse_date (tokens [1]));
}
//............................................................................

bool
tzs_equivalent (std::string const & tz_lhs, std::string const & tz_rhs)
{
    if (tz_lhs == tz_rhs) return true; // fast case

    {
        ::icu::UnicodeString us_canonical_lhs { };
        {
            UErrorCode ec { };
            ::icu::TimeZone::getCanonicalID (::icu::UnicodeString::fromUTF8 (tz_lhs.c_str ()), us_canonical_lhs, ec);
            if (! U_SUCCESS (ec))
                return false;
        }
        ::icu::UnicodeString us_canonical_rhs { };
        {
            UErrorCode ec { };
            ::icu::TimeZone::getCanonicalID (::icu::UnicodeString::fromUTF8 (tz_rhs.c_str ()), us_canonical_rhs, ec);
            if (! U_SUCCESS (ec))
                return false;
        }

        return (us_canonical_lhs == us_canonical_rhs);
    }
}
//............................................................................
//............................................................................
namespace
{

VR_ASSUME_COLD std::string
query_icu_default_tz ()
{
    UErrorCode ec { };
    string_literal_t const v = ::icu::TimeZone::getTZDataVersion (ec);
    if ((v != nullptr) && U_SUCCESS (ec))
        LOG_info << "using libicu tz data v" << v;
    else
        LOG_warn << "libicu tz data version query failed (rc " << ec << "): " << u_errorName (ec);

    std::unique_ptr<::icu::TimeZone> const icu_tz { ::icu::TimeZone::createDefault () };
    ::icu::UnicodeString us { };

    icu_tz->getID (us);

    std::string r { };
    us.toUTF8String (r);

    return r;
}

VR_ASSUME_COLD std::string
query_etc_localtime ()
{
    boost::system::error_code ec { };
    fs::path const p = fs::read_symlink ("/etc/localtime", ec);

    if (! ec)
    {
        string_vector p_elements;

        for (auto i = p.rbegin (); i != p.rend (); ++ i) // note: reverse iteration
        {
            p_elements.emplace_back (i->string ());
            if (p_elements.size () == 2) break;
        }

        if (p_elements.size () == 2)
            return join_as_name<'/'> (p_elements [1], p_elements [0]);
    }

    return { };
}
//............................................................................

struct lookup_local_tz final: public util::singleton_constructor<std::string>
{
    lookup_local_tz (std::string * const obj)
    {
        // query both ICU and '/etc/localtime'; if they aren't equivalent (which can happen
        // with single-token zones like 'UTC') issue a warning but select ICU:

        new (obj) std::string { query_icu_default_tz () };
        std::string & icu_tz = (* obj);

        {
            std::string const sys_tz = query_etc_localtime ();
            LOG_trace1 << "local tz queries: libicu -> " << print (icu_tz) << ", system -> " << print (sys_tz);

            if (VR_UNLIKELY (icu_tz.empty ()))
            {
                LOG_warn << "libicu query local tz returned an empty string, setting tz to system " << print (sys_tz);
                icu_tz = sys_tz;
            }
            else if (! tzs_equivalent (icu_tz, sys_tz))
            {
                // see if they are equivalent if we just look at the last 'sys_tz' token:

                auto const sys_slash = sys_tz.find_last_of ('/');
                if ((sys_slash != std::string::npos) && ! tzs_equivalent (icu_tz, sys_tz.substr (sys_slash + 1)))
                    LOG_warn << "system tz setting (" << print (sys_tz) << ") disagrees with libicu (" << print (icu_tz) << "): selecting libicu";
            }
        }
    }

}; // end of class

} // end of anonymous
//............................................................................
//............................................................................

std::string const &
local_tz ()
{
    return util::singleton<std::string, lookup_local_tz>::instance ();
}
//............................................................................
//............................................................................
namespace
{
#if VR_USE_BOOST_LOCALE

namespace bl    = boost::locale;
namespace blp   = boost::locale::period;

struct NY_tz_offset_impl
{
    timestamp_t operator() (date_t const & date) const
    {
        bl::date_time_period_set const bl_dt { m_zero_time + blp::year (date.year ()) + blp::month (date.month () -/* ! */1) + blp::day (date.day ()) };

        return ((bl::date_time { bl_dt, m_cal_UTC } - bl::date_time { bl_dt, m_cal_NY }) / blp::second ()) * _1_second ();
    }

    std::locale const m_loc      { bl::generator { }("en_US") };
    bl::calendar const m_cal_UTC { m_loc, "UTC" };
    bl::calendar const m_cal_NY  { m_loc, "America/New_York" };
    bl::date_time_period_set const m_zero_time { blp::hour (0) + blp::minute (0) };

}; // end of class

struct tz_offset_impl
{
    timestamp_t operator() (date_t const & date, std::string const & tz) const
    {
        bl::calendar const cal_tz { m_loc, tz };
        bl::date_time_period_set const bl_dt { m_zero_time + blp::year (date.year ()) + blp::month (date.month () -/* ! */1) + blp::day (date.day ()) };

        return ((bl::date_time { bl_dt, m_cal_UTC } - bl::date_time { bl_dt, cal_tz }) / blp::second ()) * _1_second ();
    }

    std::locale const m_loc      { bl::generator { }("en_US") };
    bl::calendar const m_cal_UTC { m_loc, "UTC" };
    bl::date_time_period_set const m_zero_time { blp::hour (0) + blp::minute (0) };

}; // end of class

#else // VR_USE_BOOST_LOCALE

struct NY_tz_offset_impl
{
    timestamp_t operator() (date_t const & date) const
    {
        UDate const ud = (ptime_t { date, pt::hours { 9 } + pt::seconds { 0 } } - epoch_time ()).total_milliseconds (); // [ms]

        int32_t raw_offset { };
        int32_t DST_offset { };

        UErrorCode ec { };

        m_icu_tz->getOffset (ud, /* local */true, raw_offset, DST_offset, ec);
        if (VR_UNLIKELY (U_FAILURE (ec)))
            throw_x (invalid_input, "NY tz offset query on " + print (date) + " failed: " + u_errorName (ec));

        DLOG_trace1 << "\t" << print (date) << " => raw offset: " << raw_offset << ", DST offset: " << DST_offset;

        return ((raw_offset + DST_offset) * _1_millisecond ());
    }

    std::unique_ptr<::icu::TimeZone> const m_icu_tz { ::icu::TimeZone::createTimeZone (::icu::UnicodeString::fromUTF8 ("America/New_York")) };

}; // end of class

struct tz_offset_impl
{
    // TODO cache more:

    timestamp_t operator() (date_t const & date, std::string const & tz) const
    {
        ::icu::UnicodeString icu_tz_canonical { };

        // validate/canonicalize 'tz' (otherwise for an invalid 'tz' string offsets will (silently) be all 0s):
        {
            UErrorCode ec { };
            UBool is_system { FALSE };

            ::icu::TimeZone::getCanonicalID (::icu::UnicodeString::fromUTF8 (tz), icu_tz_canonical, is_system, ec);

            // for some reason, the 'ec' check by itself is not enough as documented by libicu:

            if (VR_UNLIKELY (! static_cast<bool> (is_system) | (ec == U_ILLEGAL_ARGUMENT_ERROR)))
                throw_x (invalid_input, "invalid tz name " + print (tz));
        }

        std::unique_ptr<::icu::TimeZone> const icu_tz { ::icu::TimeZone::createTimeZone (icu_tz_canonical) };

        UDate const ud = (ptime_t { date, pt::hours { 9 } + pt::seconds { 0 } } - epoch_time ()).total_milliseconds (); // [ms]

        int32_t raw_offset { };
        int32_t DST_offset { };

        {
            UErrorCode ec { };

            icu_tz->getOffset (ud, /* local */true, raw_offset, DST_offset, ec);
            if (VR_UNLIKELY (U_FAILURE (ec)))
                throw_x (invalid_input, "tz offset query on " + print (date) + " in " + print (tz) + " failed: " + u_errorName (ec));
        }

        DLOG_trace1 << "\t" << print (date) << ' ' << tz << " => raw offset: " << raw_offset << ", DST offset: " << DST_offset;

        return ((raw_offset + DST_offset) * _1_millisecond ());
    }

}; // end of class

#define VR_CHECKED_ICU_CALL(exp) \
    ({ \
       UErrorCode ec { }; auto rc = (exp); \
       if (VR_UNLIKELY (U_FAILURE (ec))) \
           { throw_x (invalid_input, std::string { "(" #exp ") failed: " } + u_errorName (ec)); }; \
       rc; \
    }) \
    /* */

struct current_date_impl
{
    // TODO cache more:

    date_t operator() (std::string const & tz) const
    {
        ::icu::UnicodeString icu_tz_canonical { };

        // validate/canonicalize 'tz' (otherwise for an invalid 'tz' string offsets will (silently) be all 0s):
        {
            UErrorCode ec { };
            UBool is_system { FALSE };

            ::icu::TimeZone::getCanonicalID (::icu::UnicodeString::fromUTF8 (tz), icu_tz_canonical, is_system, ec);

            // for some reason, the 'ec' check by itself is not enough as documented by libicu:

            if (VR_UNLIKELY (! static_cast<bool> (is_system) | (ec == U_ILLEGAL_ARGUMENT_ERROR)))
                throw_x (invalid_input, "invalid tz name " + print (tz));
        }

        std::unique_ptr<::icu::TimeZone> const icu_tz { ::icu::TimeZone::createTimeZone (icu_tz_canonical) };
        {
            UErrorCode ec { };
            ::icu::GregorianCalendar cal { * icu_tz, ec };
            if (VR_UNLIKELY (U_FAILURE (ec)))
                throw_x (invalid_input, "failed to create a Gregorian calendar in " + print (tz) + ": " + u_errorName (ec));

            int32_t const year = VR_CHECKED_ICU_CALL (cal.get (UCAL_YEAR, ec));
            int32_t const month = VR_CHECKED_ICU_CALL (cal.get (UCAL_MONTH, ec)); // NOTE: 0-based
            int32_t const day = VR_CHECKED_ICU_CALL (cal.get (UCAL_DAY_OF_MONTH, ec));; // note: 1-based

            return { static_cast<date_t::year_type> (year), static_cast<date_t::month_type> (gd::Jan + month), static_cast<date_t::day_type> (day) };
        }
    }

}; // end of class


#endif // VR_USE_BOOST_LOCALE
}
//............................................................................
//............................................................................

timestamp_t
NY_tz_offset (date_t const & date)
{
    return singleton<NY_tz_offset_impl>::instance ()(date);
}

timestamp_t
tz_offset (date_t const & date, std::string const & tz)
{
    return singleton<tz_offset_impl>::instance ()(date, tz);
}
//............................................................................

date_t
current_date_in (std::string const & tz)
{
    return singleton<current_date_impl>::instance ()(tz);
}
//............................................................................

ptime_t
current_time_in (std::string const & tz)
{
    return to_ptime (sys::realtime_utc () + tz_offset (current_date_in (tz), tz)); // TODO a small chance of midnight clock race
}

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
