#pragma once

#include "vr/asserts.h"

#if ! defined (VR_USE_BOOST_LOCALE)
#   define VR_USE_BOOST_LOCALE              0
#endif

#include <boost/chrono.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>

#if defined (BOOST_DATE_TIME_HAS_NANOSECONDS)
#   define VR_BOOST_DATE_TIME_NANOSECONDS   1
#else
#   define VR_BOOST_DATE_TIME_NANOSECONDS   0
#endif

#include <tuple>

//----------------------------------------------------------------------------
namespace vr
{
namespace chrono    = boost::chrono;
namespace pt        = boost::posix_time;
namespace gd        = boost::gregorian;

namespace util
{
//............................................................................

using date_t                = gd::date;
using ptime_t               = pt::ptime;
using time_diff_t           = pt::time_duration;

using day_iterator          = gd::day_iterator;

using subsecond_duration_us = boost::date_time::subsecond_duration<pt::time_duration,    1000000>;
using subsecond_duration_ns = boost::date_time::subsecond_duration<pt::time_duration, 1000000000>;

//............................................................................

extern date_t const &
epoch_date ();

extern ptime_t const &
epoch_time ();

//............................................................................
//............................................................................
namespace
{
/*
 * trigger eager initialization from any compilation unit that includes this header
 */
date_t  const & g_epoch_date VR_UNUSED { epoch_date () };
ptime_t const & g_epoch_time VR_UNUSED { epoch_time () };

constexpr timestamp_t timestamp_max () { return 2082758400000000000L; } // start of 2036

} // end of anonymous
//............................................................................
//............................................................................

inline timestamp_t
to_timestamp (call_traits<ptime_t>::param ptime)
{
    assert_within (static_cast<int32_t> (ptime.date ().year ()), 2036); // guard against 2038 overflow problem in boost.date_time

    auto const epoch_offset = ptime - g_epoch_time;

    return (epoch_offset.total_seconds () * 1000000000UL
        + epoch_offset.fractional_seconds () * VR_IF_THEN_ELSE (VR_BOOST_DATE_TIME_NANOSECONDS)(1, 1000));
}

inline timestamp_t
to_timestamp (call_traits<date_t>::param date, timestamp_t const tod)
{
    assert_within (static_cast<int32_t> (date.year ()), 2036); // guard against 2038 overflow problem in boost.date_time

    auto const epoch_offset = date - g_epoch_date;

    return (epoch_offset.days () * (24 * 3600 * 1000000000UL) + tod);
}
//............................................................................

inline ptime_t
to_ptime (timestamp_t const ts_utc)
{
    assert_within (ts_utc, timestamp_max ()); // guard against 2038 overflow problem in boost.date_time

    return { g_epoch_date, pt::seconds { ts_utc / 1000000000 } + subsecond_duration_ns { ts_utc % 1000000000 } };
}

inline ptime_t
to_ptime (date_t const & date, timestamp_t const tod)
{
    return { date, pt::seconds { tod / 1000000000 } + subsecond_duration_ns { tod % 1000000000 } };
}
//............................................................................

extern void
print_timestamp_as_ptime (std::ostream & os, timestamp_t const ts_utc, int32_t const digits_secs = VR_IF_THEN_ELSE (VR_BOOST_DATE_TIME_NANOSECONDS)(9, 6));

inline std::string
print_timestamp_as_ptime (timestamp_t const ts_utc, int32_t const digits_secs = VR_IF_THEN_ELSE (VR_BOOST_DATE_TIME_NANOSECONDS)(9, 6))
{
    std::stringstream ss { };
    print_timestamp_as_ptime (ss, ts_utc, digits_secs);

    return ss.str ();
}
//............................................................................

extern void
format_date (call_traits<date_t>::param date, std::string const & format, std::ostream & os);

inline std::string
format_date (call_traits<date_t>::param date, std::string const & format)
{
    std::stringstream ss { };
    format_date (date, format, ss);

    return ss.str ();
}

extern void
format_time (call_traits<ptime_t>::param ptime, std::string const & format, std::ostream & os);

inline std::string
format_time (call_traits<ptime_t>::param ptime, std::string const & format)
{
    std::stringstream ss { };
    format_time (ptime, format, ss);

    return ss.str ();
}
//............................................................................

template<bool SAVE_IS_STATE>
ptime_t
parse_ptime (std::istream & is);

/**
 * @param start [note: not required to be null-terminated]
 */
extern ptime_t
parse_ptime (char_const_ptr_t const start, int32_t const size);

inline ptime_t
parse_ptime (std::string const & s)
{
    return parse_ptime (s.data (), s.size ());
}
//............................................................................

/**
 * @param start [note: not required to be null-terminated]
 */
extern timestamp_t
parse_ptime_as_timestamp (char_const_ptr_t const start, int32_t const size);

//............................................................................
/**
 * @param start char range in form "HHMMSSss...s", where the second fraction is 'digits_secs' chars long
 * @param digits_secs [non-negative]
 *
 * @note no '.' fractional second separator
 */
extern timestamp_t
parse_HHMMSS (char_const_ptr_t const start, int32_t const digits_secs);

//............................................................................
/**
 * @param start [note: not required to be null-terminated]
 */
extern timestamp_t
parse_duration_as_timestamp (char_const_ptr_t const start, int32_t const size);

//............................................................................
/**
 * looks for a 'yyyymmdd' match in 's' that parses as a valid date
 *
 * @return first such date, if found ['not-a-date-time' otherwise]
 */
extern date_t
extract_date (std::string const & s);

/**
 * attempts to parse 's' as a delimited date string, then as an undelimited string
 */
extern date_t
parse_date (std::string const & s);

/**
 * attempts to parse 's' as two date strings separated by [-, ]
 */
extern std::tuple<date_t, date_t>
parse_date_range (std::string const & s);

//............................................................................
/**
 * @return 'true' iff both timezone IDs are valid and are in the same equivalency group
 *         (i.e. have the same UTC offsets and rules)
 */
extern bool
tzs_equivalent (std::string const & tz_lhs, std::string const & tz_rhs);

//............................................................................
/**
 * infer local tz ID via the first successful lookup in this sequence:
 *  1. what '/etc/localtime' points to;
 *  2. query libicu default
 */
extern std::string const &
local_tz ();

//............................................................................
/**
 * @return NYC timezone offset on a given 'date' relative to UTC (e.g. if "America/New_York" is currently UTC-5,
 * will return '-5 * 3600 * _1_second ()'
 */
extern timestamp_t
NY_tz_offset (date_t const & date);

/**
 * @param date
 * @param tz
 * @return 'tz' offset on a given 'date' relative to UTC (UTC+X will return 'X * 3600 * _1_second ()')
 */
extern timestamp_t
tz_offset (date_t const & date, std::string const & tz);

//............................................................................
/**
 * @param tz
 */
extern date_t
current_date_in (std::string const & tz);

inline date_t
current_date_local ()
{
    return current_date_in (local_tz ());
}
//............................................................................
/**
 * @note NOTE this should not be used in a fast path (tz lookup is slow)
 *
 * @param tz
 */
extern ptime_t
current_time_in (std::string const & tz);

inline ptime_t
current_time_local ()
{
    return current_time_in (local_tz ());
}
//............................................................................

inline int32_t
date_as_int (date_t const & date)
{
    vr_static_assert (static_cast<int32_t> (gd::Jan) == 1);

    return (date.year () * 10000 + date.month () * 100 + date.day ());
}

} // end of 'util'
//............................................................................
//............................................................................
/*
 * a helper stream manipulator equivalent to 'print_timestamp_as_ptime(os, ts_uts + tz_offset, 9)'
 */
struct print_timestamp final
{
    print_timestamp (timestamp_t const ts_utc, timestamp_t const tz_offset = 0) :
        m_ts { ts_utc + tz_offset }
    {
    }

    friend std::ostream & operator<< (std::ostream & os, print_timestamp const & obj) VR_NOEXCEPT
    {
        util::print_timestamp_as_ptime (os, obj.m_ts, 9);
        return os;
    }

    timestamp_t const m_ts;

}; // end of class

} // end of namespace
//----------------------------------------------------------------------------
