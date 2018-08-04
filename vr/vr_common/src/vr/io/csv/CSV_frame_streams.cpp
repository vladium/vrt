
#include "vr/io/csv/CSV_frame_streams.h"

#include "vr/data/dataframe.h"
#include "vr/data/NA.h"
#include "vr/io/parse/CSV_tokenizer.h"
#include "vr/util/datetime.h"
#include "vr/util/format.h"
#include "vr/util/logging.h"
#include "vr/util/parse.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
using namespace data;

//............................................................................
//............................................................................
namespace
{

inline int32_t
set_precision (int32_t const cfg)
{
    return (cfg > 0 ? cfg : (cfg < 0 ? std::numeric_limits<double>::digits10 + 1 : -1));
}
//............................................................................

struct read_visitor final // stateless
{
    VR_FORCEINLINE void
    operator() (atype_<atype::category>,
                attribute const & a, addr_t const col_raw, dataframe::size_type const row, parse::CSV_token const & t) const
    {
        enum_int_t & x = static_cast<enum_int_t *> (col_raw) [row];

        if (VR_UNLIKELY (t.m_type == parse::CSV_token::NA_token))
            mark_NA (x); // note that array_label::index () will handle an "NA" string as well (but won't deal with quotes)
        else
        {
            check_eq (t.m_type, parse::CSV_token::quoted_name);

            x = a.labels ().index (t.m_start +/* quote */1, t.m_size -/* quotes */2);
        }
    }

    VR_FORCEINLINE void
    operator() (atype_<atype::timestamp>,
                attribute const & a, addr_t const col_raw, dataframe::size_type const row, parse::CSV_token const & t) const
    {
        timestamp_t & x = static_cast<timestamp_t *> (col_raw) [row];

        if (VR_UNLIKELY (t.m_type == parse::CSV_token::NA_token))
            mark_NA (x);
        else
        {
            check_eq (t.m_type, parse::CSV_token::datetime);

            x = util::parse_ptime_as_timestamp (t.m_start, t.m_size);
        }
    }

    // TODO std::from_chars() [c++17]

    VR_FORCEINLINE void
    operator() (atype_<atype::i4>,
                attribute const & a, addr_t const col_raw, dataframe::size_type const row, parse::CSV_token const & t) const
    {
        int32_t & x = static_cast<int32_t *> (col_raw) [row];

        if (VR_UNLIKELY (t.m_type == parse::CSV_token::NA_token))
            mark_NA (x);
        else
        {
            check_eq (t.m_type, parse::CSV_token::num_int);

            x = util::parse_decimal<int32_t> (t.m_start, t.m_size);
        }
    }

    VR_FORCEINLINE void
    operator() (atype_<atype::i8>,
                attribute const & a, addr_t const col_raw, dataframe::size_type const row, parse::CSV_token const & t) const
    {
        int64_t & x = static_cast<int64_t *> (col_raw) [row];

        if (VR_UNLIKELY (t.m_type == parse::CSV_token::NA_token))
            mark_NA (x);
        else
        {
            check_eq (t.m_type, parse::CSV_token::num_int);

            x = util::parse_decimal<int64_t> (t.m_start, t.m_size);
        }
    }

    VR_FORCEINLINE void
    operator() (atype_<atype::f4>,
                attribute const & a, addr_t const col_raw, dataframe::size_type const row, parse::CSV_token const & t) const
    {
        float & x = static_cast<float *> (col_raw) [row];

        if (VR_UNLIKELY (t.m_type == parse::CSV_token::NA_token))
            mark_NA (x);
        else
        {
            check_condition (((1 << t.m_type) & meta::static_bitset<bitset32_t, parse::CSV_token::num_int, parse::CSV_token::num_fp>::value), t.m_type);

            VR_IF_DEBUG (int32_t const rc =) std::sscanf (t.m_start, "%f", & x);
            assert_eq (rc, 1);
        }
    }

    VR_FORCEINLINE void
    operator() (atype_<atype::f8>,
                attribute const & a, addr_t const col_raw, dataframe::size_type const row, parse::CSV_token const & t) const
    {
        double & x = static_cast<double *> (col_raw) [row];

        if (VR_UNLIKELY (t.m_type == parse::CSV_token::NA_token))
            mark_NA (x);
        else
        {
            check_condition (((1 << t.m_type) & meta::static_bitset<bitset32_t, parse::CSV_token::num_int, parse::CSV_token::num_fp>::value), t.m_type);

            VR_IF_DEBUG (int32_t const rc =) std::sscanf (t.m_start, "%lf", & x);
            assert_eq (rc, 1);
        }
    }

}; // end of class


struct write_visitor final // stateless
{
    VR_FORCEINLINE void
    operator() (atype_<atype::category>,
                attribute const & a, addr_const_t const col_raw, dataframe::size_type const row, std::ostream & os, int32_t const) const
    {
        enum_int_t const & x = static_cast<enum_int_t const *> (col_raw) [row];

        if (VR_UNLIKELY (is_NA (x)))
            os << VR_ENUM_NA_NAME; // TODO parameterize this? (empty string, config parm, etc)
        else
            os << '"' << a.labels ()[x] << '"';
    }

    VR_FORCEINLINE void
    operator() (atype_<atype::timestamp>,
                attribute const & a, addr_const_t const col_raw, dataframe::size_type const row, std::ostream & os, int32_t const digits_secs) const
    {
        timestamp_t const & x = static_cast<timestamp_t const *> (col_raw) [row];

        if (VR_UNLIKELY (is_NA (x)))
            os << VR_ENUM_NA_NAME; // TODO parameterize this? (empty string, config parm, etc)
        else
        {
            os << util::ptime_t { util::g_epoch_date, pt::seconds { x / 1000000000 } }; // NOTE: stream constructor sets a facet to only print up to seconds

            if (VR_LIKELY (digits_secs > 0))
            {
                char buf [10] = { '.', '0', '0', '0', '0', '0', '0', '0', '0', '0' };
                util::rjust_print_decimal_nonnegative (x % 1000000000, buf + 1, 9);

                os.write (buf, 1 + digits_secs);
            }
        }
    }

    template<atype::enum_t ATYPE>
    VR_FORCEINLINE void
    operator() (atype_<ATYPE>,
                attribute const & a, addr_const_t const col_raw, dataframe::size_type const row, std::ostream & os, int32_t const) const
    {
        using data_type             = typename atype::traits<ATYPE>::type;

        data_type const & x = static_cast<data_type const *> (col_raw) [row];

        if (VR_UNLIKELY (is_NA (x)))
            os << VR_ENUM_NA_NAME; // TODO parameterize this? (empty string, config parm, etc)
        else
            os << x;
    }

}; // end of class

} // end of anonymous
//............................................................................
//............................................................................
namespace impl
{

CSV_frame_istream_base::CSV_frame_istream_base (arg_map const & parms, std::unique_ptr<std::istream> && is) :
    m_is { std::move (is) },
    m_header_row_count { parms.get<int32_t> ("header", 1) }
    /*m_separator { parms.get<char> ("sep", ',') }*/
{
    LOG_trace1 << "configured with header row count " << m_header_row_count;
    check_nonnegative (m_header_row_count);
}

void
CSV_frame_istream_base::initialize (attr_schema::ptr const & schema)
{
    assert_condition (! m_schema);

    std::istream & is = * m_is;
    attr_schema const & as = * (m_schema = schema);

    LOG_trace1 << "CSV istream initialized with schema\n" << print (as);

    if (m_header_row_count > 0)
    {
        // if a row header is expected, reconcile column names with 'schema':

        if (VR_UNLIKELY (! std::getline (is, m_line)))
            throw_x (io_exception, "I/O failure reading CSV header");


        width_type const col_count = parse::CSV_tokenize (m_line, m_tokens);
        m_tokens.shrink_to_fit ();

        if (VR_UNLIKELY (as.size () != col_count))
            throw_x (io_exception, "expected " + print (as.size ()) + " header column(s) instead of " + print (col_count) + ": '" + m_line + '\'');

        for (width_type c = 0, c_limit = as.size (); c < c_limit; ++ c)
        {
            attribute const & a = as [c];

            check_eq (m_tokens [c].m_type, parse::CSV_token::quoted_name);

            if (VR_UNLIKELY (signed_cast (a.name ().size ()) != m_tokens [c].m_size -/* quotes */2))
                throw_x (io_exception, "expected column " + print (c + 1) + " to have name " + print (a.name ()) +
                    " instead of " + print (std::string { m_tokens [c].m_start + 1, static_cast<std::size_t> (m_tokens [c].m_size - 2) }));

            if (VR_UNLIKELY (std::strncmp (a.name ().c_str (), m_tokens [c].m_start +/* quote */1, a.name ().size ())))
                throw_x (io_exception, "expected column " + print (c + 1) + " to have name " + print (a.name ()) +
                    " instead of " + print (std::string { m_tokens [c].m_start + 1, static_cast<std::size_t> (m_tokens [c].m_size - 2) }));
        }

        // eat remaining header rows:

        for (int32_t r = /* ! */1; r < m_header_row_count; ++ r)
        {
            if (VR_UNLIKELY (! std::getline (is, m_line)))
                throw_x (io_exception, "I/O failure reading CSV header");
        }
    }
}

void
CSV_frame_istream_base::close ()
{
    m_is.reset ();
}
//............................................................................

std::istream &
CSV_frame_istream_base::stream ()
{
    check_condition (m_is);
    return (* m_is);
}
//............................................................................

int64_t
CSV_frame_istream_base::read (dataframe & dst)
{
    if (VR_UNLIKELY (! m_is)) // guard against using a close()d stream
        return -1;

    if (VR_UNLIKELY (! m_schema))
    {
        initialize (dst.schema ());
        assert_condition (!! m_schema);
    }
    else if (VR_UNLIKELY (m_last_io_df != & dst))
    {
        // an imprecise, but fast, schema consistency check:

        check_eq (dst.schema ()->size (), m_schema->size ());
        assert_eq (hash_value (* dst.schema ()), hash_value (* m_schema));
    }

    dataframe::size_type const dst_row_capacity = dst.row_capacity ();

    std::istream & is = * m_is;
    attr_schema const & as = * m_schema;

    width_type const col_count { as.size () };

    dataframe::size_type r;
    for (r = 0; r < dst_row_capacity; ++ r)
    {
        if (VR_UNLIKELY (! std::getline (is, m_line)))
        {
            if (VR_LIKELY (is.eof ()))
            {
                LOG_trace1 << "EOF, closing the stream";
                close ();
                break;
            }
            else
                throw_x (io_exception, "I/O failure"); // TODO more details (util method for showing which bad bits are set on a stream)
        }

        m_tokens.clear ();
        width_type const token_count = parse::CSV_tokenize (m_line, m_tokens);
        check_eq (token_count, col_count, r, m_line);

        for (width_type c = 0; c < col_count; ++ c)
        {
            attribute const & a = as [c];

            dispatch_on_atype (a.atype (), read_visitor { }, a, dst.at_raw<false> (c), r, m_tokens [c]); // TODO check inlining
        }
    }

    dst.resize_row_count (r);

    m_last_io_df = & dst; // TODO can also cache col raw addrs
    return r;
}
//............................................................................

CSV_frame_ostream_base::CSV_frame_ostream_base (arg_map const & parms, std::unique_ptr<std::ostream> && os) :
    m_os { std::move (os) },
    m_digits { set_precision (parms.get<int32_t> ("digits", /* -1: max, default: leave at default */0)) },
    m_digits_secs { parms.get<int32_t> ("digits.secs", /* default to ns */9) },
    m_separator { parms.get<char> ("sep", ',') }
{
    LOG_trace1 << "configured with " << m_digits << " digits, " << m_digits_secs << " digits.secs";

    check_within (m_digits_secs, 10);

    // because we own 'm_os', not going to restore these settings on destruction:

    m_os->imbue (std::locale { m_os->getloc (), new pt::time_facet { "%Y-%b-%d %H:%M:%S" } }); // note: locale obj takes facet ownership

    m_os->setf (std::ios::fixed);
    if (m_digits >= 0) m_os->precision (m_digits);
}

void
CSV_frame_ostream_base::initialize (attr_schema::ptr const & schema)
{
    assert_condition (! m_schema);

    std::ostream & os = * m_os;
    attr_schema const & as = * (m_schema = schema);

    LOG_trace1 << "CSV ostream initialized with schema\n" << print (as);

    for (width_type c = 0, c_limit = as.size (); c < c_limit; ++ c)
    {
        attribute const & a = as [c];

        if (VR_LIKELY (c)) os << m_separator;
        os << print (a.name ());
    }
    os.put ('\n');
}

void
CSV_frame_ostream_base::close ()
{
    if (m_os)
    {
        m_os->flush ();
        m_os.reset ();
    }
}
//............................................................................

std::ostream &
CSV_frame_ostream_base::stream ()
{
    check_condition (m_os);
    return (* m_os);
}
//............................................................................

int64_t
CSV_frame_ostream_base::write (dataframe const & src)
{
    check_condition (!! m_os); // guard against using a close()d stream

    if (VR_UNLIKELY (! m_schema))
    {
        initialize (src.schema ());
        assert_condition (!! m_schema);
    }
    else if (VR_UNLIKELY (m_last_io_df != & src))
    {
        // an imprecise, but fast, schema consistency check:

        check_eq (src.schema ()->size (), m_schema->size ());
        assert_eq (hash_value (* src.schema ()), hash_value (* m_schema));
    }

    dataframe::size_type const src_row_count = src.row_count ();
    assert_positive (src_row_count);

    std::ostream & os = * m_os;
    attr_schema const & as = * m_schema;

    width_type const col_count { as.size () };
    int32_t const digits_secs = m_digits_secs;

    for (dataframe::size_type r = 0; r < src_row_count; ++ r)
    {
        for (width_type c = 0; c < col_count; ++ c)
        {
            attribute const & a = as [c];

            if (VR_LIKELY (c)) os << m_separator;
            dispatch_on_atype (a.atype (), write_visitor { }, a, src.at_raw<false> (c), r, os, digits_secs); // TODO check inlining
        }
        os.put ('\n');
    }

    m_last_io_df = & src; // TODO can also cache col raw addrs
    return src_row_count;
}

} // end of 'impl'
//............................................................................
//............................................................................
} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
