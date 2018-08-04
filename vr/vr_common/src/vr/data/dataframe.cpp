
#include "vr/data/dataframe.h"

#include "vr/io/streams.h"
#include "vr/util/datetime.h"
#include "vr/util/format.h"
#include "vr/util/logging.h"
#include "vr/util/memory.h"
#include "vr/util/ops_int.h"

#include <boost/format.hpp>

#include <algorithm>
#include <cstring>

//----------------------------------------------------------------------------
namespace vr
{
namespace data
{
//............................................................................

vr_static_assert (util::has_print<dataframe>::value);

//............................................................................
//............................................................................
namespace
{

using ops_unchecked     = util::ops_int<util::arg_policy<util::zero_arg_policy::ignore, 0>, false>;

} // end of anonymous
//............................................................................
//............................................................................

dataframe::dataframe (size_type const row_capacity, attr_schema && schema) :
    m_schema { new attr_schema { std::move (schema) } },
    m_log2_row_capacity { ops_unchecked::log2_floor ([row_capacity] { check_is_power_of_2 (row_capacity); return row_capacity; } ()) },
    m_data { allocate_data (* m_schema, m_log2_row_capacity VR_IF_DEBUG (, m_data_alloc_size)) }
{
}

dataframe::dataframe (size_type const row_capacity, attr_schema::ptr const & schema) :
    m_schema { [& schema] { check_condition (schema); return schema; } () },
    m_log2_row_capacity { ops_unchecked::log2_floor ([row_capacity] { check_is_power_of_2 (row_capacity); return row_capacity; } ()) },
    m_data { allocate_data (* m_schema, m_log2_row_capacity VR_IF_DEBUG (, m_data_alloc_size)) }
{
}
//............................................................................

inline dataframe::data_ptr // local linkage
dataframe::allocate_data (attr_schema const & as, size_type const log2_row_capacity VR_IF_DEBUG(, size_type & data_alloc_size))
{
    LOG_trace1 << "allocating column data for schema\n" << as;

    auto const col_count = as.size ();

    int32_t const bitmap_bitsize    = 8 * sizeof (bitmap_t);
    auto const block_count          = (col_count + bitmap_bitsize - 1) / bitmap_bitsize;

    // bitmap header size ['bitmap_t' units]:

    size_type sz = 2 * block_count;

    // walk the schema in 'bitmap_t' blocks:

    std::unique_ptr<bitmap_t []> const header { boost::make_unique_noinit<bitmap_t []> (sz) };
    attr_schema::size_type index { };

    for (int32_t b = 0; b < block_count; ++ b)
    {
        bitmap_t bm { };

        const int32_t i_limit = std::min<int32_t> (col_count - index, bitmap_bitsize);
        for (int32_t i = 0; i < i_limit; ++ i, ++ index)
        {
            switch (atype::width [as [index].atype ()])
            {
                case 4: break;
                case 8: bm |= (static_cast<bitmap_t> (1) << i); break;

                default: VR_ASSUME_UNREACHABLE ();

            } // end of switch
        }

        int32_t const i8_fields = util::popcount (bm);
        if (sz & 1) sz += static_cast<bool> (i8_fields); // a padding hole may only be needed if 'log2_row_capacity' is 0

        DLOG_trace2 << "  adding bitmap block #" << b << " {" << sz << ", 0x" << std::hex << bm << '}';

        header [b * 2] = sz;
        header [b * 2 + 1] = bm;

        sz += ((i_limit + i8_fields) << log2_row_capacity);
    }

    signed_size_t const alloc_sz = sz * sizeof (bitmap_t); // [bytes]
    VR_IF_DEBUG (data_alloc_size = alloc_sz;)

    data_ptr data { static_cast<bitmap_t *> (util::aligned_alloc (8, alloc_sz)), util::aligned_free };

    LOG_trace1 << "allocated " << alloc_sz << " bytes [" << data.get () << ", " << static_cast<addr_const_t> (byte_ptr_cast (data.get ()) + alloc_sz) << ')';

    std::memcpy (VR_ASSUME_ALIGNED (data.get (), 8), header.get (), block_count * 2 * sizeof (bitmap_t));

    return data;
}
//............................................................................

void
dataframe::resize_row_count (size_type const rows)
{
    check_within_inclusive (rows, row_capacity ());

    m_row_count = rows;
}
//............................................................................
//............................................................................
namespace
{

int32_t const head_row_count    = 28;
int32_t const tail_row_count    = 3;

int32_t const precision         = 6;
int32_t const digits_secs       = 9;
int32_t const date_width        = 20 + static_cast<bool> (digits_secs) + digits_secs;

using timestamp_stream          = io::array_ostream<>;

//............................................................................

char const g_div_eol [] = "------------------------------------------------------------|";

//............................................................................

template<typename T>
constexpr int32_t
select_width_for_type (int32_t const request)
{
    return (request >= 0 ? request : std::numeric_limits<T>::digits10 + 1);
}
//............................................................................

struct make_format final // stateless
{
    void
    operator() (atype_<atype::category>,
                attribute const & a, int32_t & pos, std::stringstream & fmt_head, std::stringstream & fmt, std::stringstream & div) const
    {
        int32_t const w = std::max<int32_t> (std::max<int32_t> (a.name ().size (), std::strlen (atype::name (atype::category))), a.labels ().max_label_size ());

        fmt_head    << "%|" << pos << "t|%|=" << w << "||";
        fmt         << "%|" << pos << "t|%|=" << w << "||";

        int32_t const pos_step = w + 1;

        div.write (g_div_eol + sizeof (g_div_eol) - 1 - pos_step, pos_step);

        pos += pos_step;
    }

    void
    operator() (atype_<atype::timestamp>,
                attribute const & a, int32_t & pos, std::stringstream & fmt_head, std::stringstream & fmt, std::stringstream & div) const
    {
        int32_t const w = std::max<int32_t> (std::max<int32_t> (a.name ().size (), std::strlen (atype::name (atype::timestamp))), 2 + date_width);

        fmt_head    << "%|" << pos << "t|%|=" << w << "||";
        fmt         << "%|" << pos << "t|%| -" << w << "||";

        int32_t const pos_step = w + 1;

        div.write (g_div_eol + sizeof (g_div_eol) - 1 - pos_step, pos_step);

        pos += pos_step;
    }

    template<atype::enum_t ATYPE, atype::enum_t _ = ATYPE>
    void
    operator() (atype_<ATYPE>,
                attribute const & a, int32_t & pos, std::stringstream & fmt_head, std::stringstream & fmt, std::stringstream & div,
                typename std::enable_if<std::is_integral<typename atype::traits<ATYPE>::type>::value>::type * = { }) const
    {
        using data_type             = typename atype::traits<ATYPE>::type;

        int32_t const w = std::max<int32_t> (a.name ().size (), 1 + select_width_for_type<data_type> (-1));

        fmt_head    << "%|" << pos << "t|%|=" << (w + 1) << "||";
        fmt         << "%|" << pos << "t|%|" << w << "| |";

        int32_t const pos_step = w + 2;

        div.write (g_div_eol + sizeof (g_div_eol) - 1 - pos_step, pos_step);

        pos += pos_step;
    }

    void
    operator() (atype_<atype::f4>,
                attribute const & a, int32_t & pos, std::stringstream & fmt_head, std::stringstream & fmt, std::stringstream & div) const
    {
        int32_t const w = std::max<int32_t> (a.name ().size (), 12);
        int32_t const p = precision;

        fmt_head    << "%|" << pos << "t|%|=" << (w + 1) << "||";
        fmt         << "%|" << pos << "t|%|-' '" << w << "." << p << "g| |";

        int32_t const pos_step = w + 2;

        div.write (g_div_eol + sizeof (g_div_eol) - 1 - pos_step, pos_step);

        pos += pos_step;
    }

    void
    operator() (atype_<atype::f8>,
                attribute const & a, int32_t & pos, std::stringstream & fmt_head, std::stringstream & fmt, std::stringstream & div) const
    {
        int32_t const w = std::max<int32_t> (a.name ().size (), 13);
        int32_t const p = precision;

        fmt_head    << "%|" << pos << "t|%|=" << (w + 1) << "||";
        fmt         << "%|" << pos << "t|%|-' '" << w << "." << p << "g| |";

        int32_t const pos_step = w + 2;

        div.write (g_div_eol + sizeof (g_div_eol) - 1 - pos_step, pos_step);

        pos += pos_step;
    }

}; // end of class
//............................................................................

struct print_visitor final // stateless
{
    VR_FORCEINLINE void
    operator() (atype_<atype::category>,
                attribute const & a, addr_const_t const col_raw, dataframe::size_type const row, boost::format & fmt, timestamp_stream & dts, char (& dts_buf) [date_width + 1]) const
    {
        fmt % a.labels ()[static_cast<enum_int_t const *> (col_raw) [row]];
    }

    VR_FORCEINLINE void
    operator() (atype_<atype::timestamp>,
                attribute const & a, addr_const_t const col_raw, dataframe::size_type const row, boost::format & fmt, timestamp_stream & dts, char (& dts_buf) [date_width + 1]) const
    {
        timestamp_t const & ts = static_cast<timestamp_t const *> (col_raw) [row];

        dts.seekp (0); // reset 'dts' (note that 'clear()' will discard the facet setup)
        dts << util::ptime_t { util::g_epoch_date, pt::seconds { ts / 1000000000 } }; // NOTE: stream constructor set a facet to only print up to seconds

        if (digits_secs > 0) // compile-time branch (for now)
        {
            char buf [10] = { '.', '0', '0', '0', '0', '0', '0', '0', '0', '0' };
            util::rjust_print_decimal_nonnegative (ts % 1000000000, buf + 1, 9);

            dts.write (buf, 1 + digits_secs);
        }

        fmt % dts_buf;
    }

    template<atype::enum_t ATYPE>
    VR_FORCEINLINE void
    operator() (atype_<ATYPE>,
                attribute const & a, addr_const_t const col_raw, dataframe::size_type const row, boost::format & fmt, timestamp_stream & dts, char (& dts_buf) [date_width + 1]) const
    {
        using data_type             = typename atype::traits<ATYPE>::type;

        fmt % static_cast<data_type const *> (col_raw) [row];
    }

}; // end of class

} // end of anonymous
//............................................................................
//............................................................................

std::string
__print__ (dataframe const & obj) VR_NOEXCEPT
{
    attr_schema const & as = * obj.schema ();
    width_type const col_count = as.size ();

    char dts_buf [date_width + 1] = { 0 }; // note: the last zero remains untouched and so terminates the c-string
    timestamp_stream dts { dts_buf };  // dedicated to timestamp printing
    {
        dts.imbue (std::locale { dts.getloc (), new pt::time_facet { "%Y-%b-%d %H:%M:%S" } }); // note: locale obj takes facet ownership
    }

    std::stringstream ss;
    ss << "dataframe<" << obj.row_count () << 'x' << obj.col_count () << "> @" << & obj << ":\n";

    boost::format fmt_head;
    boost::format fmt_data;
    std::string div;
    {
        std::stringstream fmt_head_ss;
        std::stringstream fmt_data_ss;
        std::stringstream div_ss;

        fmt_head_ss.put ('|');
        fmt_data_ss.put ('|');
        div_ss.put ('|');

        int32_t pos { /* first pipe */1 };

        for (width_type c = 0; c < col_count; ++ c)
        {
            attribute const & a = as [c];

            dispatch_on_atype (a.atype (), make_format { }, a, pos, fmt_head_ss, fmt_data_ss, div_ss);
        }

        fmt_head_ss.put ('\n');
        fmt_data_ss.put ('\n');
        div_ss.put ('\n');

        fmt_head.parse (fmt_head_ss.str ());
        fmt_data.parse (fmt_data_ss.str ());
        div = div_ss.str ();
    }

    // print header:
    {
        boost::format fmt_h0 { fmt_head };
        boost::format fmt_h1 { fmt_head }; // make a copy, since we'll populate two rows in the same loop

        for (width_type c = 0; c < col_count; ++ c)
        {
            const attribute & a = as [c];

            fmt_h0 % a.name ();
            fmt_h1 % atype::name (a.atype ());
        }

        ss << fmt_h0 << fmt_h1 << div;
    }

    // print data rows:

    bool const abbreviate = (obj.row_count () > (head_row_count + tail_row_count + 1));

    int32_t const print_count = (abbreviate ? head_row_count : obj.row_count ());

    for (dataframe::size_type row = 0; row < print_count; ++ row)
    {
        boost::format fmt_row { fmt_data }; // a copy is not strictly required, but is a perf improving measure (see boost.format docs)

        for (width_type c = 0; c < col_count; ++ c)
        {
            attribute const & a = as [c];

            dispatch_on_atype (a.atype (), print_visitor { }, a, obj.at_raw<false> (c), row, fmt_row, dts, dts_buf);
        }
        ss << fmt_row;
    }

    if (abbreviate)
    {
        for (width_type c = 0; c < col_count; ++ c)
        {
            fmt_head % "...";
        }
        ss << fmt_head; // note: last use of 'fmt_head'

        for (dataframe::size_type row = obj.row_count () - tail_row_count; row < obj.row_count (); ++ row)
        {
            boost::format fmt_row { fmt_data }; // a copy is not strictly required, but is a perf improving measure (see boost.format docs)

            for (width_type c = 0; c < col_count; ++ c)
            {
                attribute const & a = as [c];

                dispatch_on_atype (a.atype (), print_visitor { }, a, obj.at_raw<false> (c), row, fmt_row, dts, dts_buf);
            }
            ss << fmt_row;
        }
    }

    return ss.str ();
}

} // end of 'data'
} // end of namespace
//----------------------------------------------------------------------------
