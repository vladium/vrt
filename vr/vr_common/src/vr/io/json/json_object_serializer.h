#pragma once

#include "vr/fw_string.h"
#include "vr/io/json/defs.h"
#include "vr/io/parse/JSON_tokenizer.h"
#include "vr/strings.h"
#include "vr/meta/objects.h"
#include "vr/util/logging.h"
#include "vr/util/parse.h"

#include <functional> // std::ref

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{

// TODO support argument-dependent lookup

/*
 * bool
 */
template<typename T>
struct json_io_traits<T,
    util::enable_if_t<util::is_bool<T>::value> >
{
    static int32_t consume (JSON_token_vector const & data, int32_t const position, bool & x)
    {
        auto const & t = data [position];
        DLOG_trace2 << "consuming " << print (t.str_range ()) << " as {bool}";
        assert_eq (t.m_type, parse::JSON_token::boolean);

        x = (t.m_start [0] == 't');

        return 1;
    }

    static std::ostream & emit (bool const x, std::ostream & os)
    {
        return os << (x ? "true" : "false");
    }

}; // end of specialization

/*
 * char (narrow only)
 */
template<typename T>
struct json_io_traits<T,
    util::enable_if_t<util::is_char<T>::value> >
{
    static int32_t consume (JSON_token_vector const & data, int32_t const position, char & x)
    {
        auto const & t = data [position];
        DLOG_trace2 << "consuming " << print (t.str_range ()) << " as {char}";
        assert_eq (t.m_type, parse::JSON_token::string);
        assert_eq (t.m_size, 3); // quoted

        x = t.m_start [1];

        return 1;
    }

    static std::ostream & emit (char const x, std::ostream & os)
    {
        return os << '"' << x << '"';
    }

}; // end of specialization

/*
 * integers
 */
template<typename T> // TODO use "util/format.h"
struct json_io_traits<T,
    util::enable_if_t<(std::is_integral<T>::value && ! (util::is_bool<T>::value || util::is_char<T>::value))> >
{
    static int32_t consume (JSON_token_vector const & data, int32_t const position, T & x)
    {
        auto const & t = data [position];
        DLOG_trace2 << "consuming " << print (t.str_range ()) << " as {" << cn_<T> () << '}';
        assert_eq (t.m_type, parse::JSON_token::num_int);

        x = util::parse_decimal<T> (t.m_start, t.m_size);

        return 1;
    }

    static std::ostream & emit (T const x, std::ostream & os)
    {
        return os << x; // 'printable'
    }

}; // end of specialization

/*
 * typesafe enums TODO static asserts for being printable and parsable
 */
template<typename T>
struct json_io_traits<T,
    util::enable_if_t<is_typesafe_enum<T>::value> >
{
    static int32_t consume (JSON_token_vector const & data, int32_t const position, typename T::enum_t & x)
    {
        auto const & t = data [position];
        DLOG_trace2 << "consuming " << print (t.str_range ()) << " as {" << cn_<T> () << '}';
        assert_eq (t.m_type, parse::JSON_token::string);
        assert_ge (t.m_size, 2); // quoted

        x = to_enum<T> (t.m_start + 1, t.m_size - 2);

        return 1;
    }

    static std::ostream & emit (typename T::enum_t const x, std::ostream & os)
    {
        return os << print (x);
    }

}; // end of specialization

/*
 * std::string
 */
template<typename T>
struct json_io_traits<T,
    util::enable_if_t<util::is_std_string<T>::value> >
{
    static int32_t consume (JSON_token_vector const & data, int32_t const position, std::string & x)
    {
        auto const & t = data [position];
        DLOG_trace2 << "consuming " << print (t.str_range ()) << " as {std::string}";
        assert_eq (t.m_type, parse::JSON_token::string);
        assert_ge (t.m_size, 2); // quoted

        x.assign (t.m_start + 1, t.m_size - 2);

        return 1;
    }

    static std::ostream & emit (std::string const & x, std::ostream & os)
    {
        return os << quote_string<'"'> (x); // TODO escape
    }

}; // end of specialization

/*
 * std::array<char, N>
 */
template<typename T>
struct json_io_traits<T,
    util::enable_if_t<util::is_std_char_array<T>::value> >
{
    static constexpr int32_t size ()    { return util::array_size<T>::value; }

    static int32_t consume (JSON_token_vector const & data, int32_t const position, T & x)
    {
        auto const & t = data [position];
        DLOG_trace2 << "consuming " << print (t.str_range ()) << " as {" << cn_<T> () << '}';
        assert_eq (t.m_type, parse::JSON_token::string);
        assert_within_inclusive (t.m_size - 2, size ()); // quoted

        std::memcpy (& x [0], t.m_start + 1, t.m_size - 2);

        return 1;
    }

    static std::ostream & emit (T const & x, std::ostream & os)
    {
        return os << quote_string<'"'> (x); // TODO escape
    }

}; // end of specialization

/*
 * fw_string<N, bool>
 */
template<typename T>
struct json_io_traits<T,
    util::enable_if_t<util::is_fw_string<T>::value> >
{
    static int32_t consume (JSON_token_vector const & data, int32_t const position, T & x)
    {
        auto const & t = data [position];
        DLOG_trace2 << "consuming " << print (t.str_range ()) << " as {" << cn_<T> () << '}';
        assert_eq (t.m_type, parse::JSON_token::string);
        assert_ge (t.m_size, 2); // quoted

        x = T { t.m_start + 1, t.m_size - 2 };

        return 1;
    }

    static std::ostream & emit (T const & x, std::ostream & os)
    {
        return os << quote_string<'"'> (x.to_string ()); // TODO escape
    }

}; // end of specialization


/*
 * std::vector<T>
 */
template<typename T>
struct json_io_traits<T, util::enable_if_t<util::is_std_vector<T>::value> >; // forward
/*
 * T [N]
 */
template<typename T>
struct json_io_traits<T, util::enable_if_t<std::is_array<T>::value> >; // forward
/*
 * std::array<T, N>, T != char
 */
template<typename T>
struct json_io_traits<T, util::enable_if_t<(util::is_std_array<T>::value && ! util::is_std_char_array<T>::value)> >; // forward

/*
 * meta-object
 */
template<typename T>
struct json_io_traits<T, util::enable_if_t<meta::is_meta_struct<T>::value> >; // forward

//............................................................................
//............................................................................
namespace impl
{

template<typename S>
struct object_consume_visitor
{
    object_consume_visitor (S & s, JSON_token_vector const & data, int32_t const position) :
        m_s { & s },
        m_data { & data },
        m_position { position }
    {
    }

    template<typename FIELD_DEF>
    void operator() (FIELD_DEF)
    {
        using tag           = typename FIELD_DEF::tag;
        using field_type    = typename FIELD_DEF::value_type; // note: 'field_type' may be a typesafe enum

        using traits        = json_io_traits<field_type>;

        // TODO assert tag name matches

        m_consumed += 1 + traits::consume (* m_data, m_position + m_consumed + 1, meta::field<tag> (* m_s));
    }

    int32_t const & consumed () const
    {
        return m_consumed;
    }

    S * const m_s;
    JSON_token_vector const * const m_data;
    int32_t const m_position;
    int32_t m_consumed { };

}; // end of class
//............................................................................

template<typename S>
struct object_emit_visitor
{
    object_emit_visitor (S const & s, std::ostream & os) :
        m_s { & s },
        m_os { & os }
    {
    }

    template<typename FIELD_DEF>
    void operator() (FIELD_DEF)
    {
        using tag           = typename FIELD_DEF::tag;
        using field_type    = typename FIELD_DEF::value_type; // note: 'field_type' may be a typesafe enum

        using traits        = json_io_traits<field_type>;

        std::ostream & os = * m_os;

        if (VR_UNLIKELY (! m_not_first))
            m_not_first = true;
        else
            os.put (',');

        meta::emit_tag_name<tag, /* QUOTE */true> (os);
        os.put (':');
        traits::emit (meta::field<tag> (* m_s), os);
    }

    S const * const m_s;
    std::ostream * const m_os;
    bool m_not_first { false };

}; // end of class

} // end of 'impl'
//............................................................................
//............................................................................
/*
 * std::vector<T>
 */
template<typename T>
struct json_io_traits<T, util::enable_if_t<util::is_std_vector<T>::value> >
{
    using value_type                    = typename T::value_type;

    using traits        = json_io_traits<value_type>;

    static int32_t consume (JSON_token_vector const & data, int32_t const position, T & x)
    {
        assert_within_inclusive (x.size (), data.size () - position);

        int32_t consumed { };
        for (int32_t i = 0, i_limit = data.size () - position; i < i_limit; ++ i)
        {
            if (data [position + consumed].m_type == parse::JSON_token::eoa)
            {
                ++ consumed;
                break;
            }

            x.emplace_back ();
            consumed += traits::consume (data, position + consumed, x.back ());
        }

        return consumed;
    }

    static std::ostream & emit (T const & x, std::ostream & os)
    {
        os.put ('[');
        for (int32_t i = 0, i_limit = x.size (); i < i_limit; ++ i)
        {
            if (i) os.put (',');
            traits::emit (x [i], os);
        }
        os.put (']');

        return os;
    }

}; // end of specialization

/*
 * T [N]
 */
template<typename T>
struct json_io_traits<T, util::enable_if_t<std::is_array<T>::value> >
{
    using value_type                    = typename std::remove_extent<T>::type;
    static constexpr int32_t sz ()      { return std::extent<T>::value; }

    using traits        = json_io_traits<value_type>;

    static int32_t consume (JSON_token_vector const & data, int32_t const position, T & x)
    {
        assert_within_inclusive (sz (), data.size () - position);

        int32_t consumed { };
        for (int32_t i = 0; i < sz (); ++ i)
        {
            consumed += traits::consume (data, position + consumed, x [i]);
        }

        if (VR_UNLIKELY ((position + consumed >= static_cast<int32_t> (data.size ())) || (data [position + consumed].m_type != parse::JSON_token::eoa)))
            throw_x (parse_failure, "expected ']'");
        else // consume end-of-array
            ++ consumed;

        return consumed;
    }

    static std::ostream & emit (T const & x, std::ostream & os)
    {
        os.put ('[');
        for (int32_t i = 0; i < sz (); ++ i)
        {
            if (i) os.put (',');
            traits::emit (x [i], os);
        }
        os.put (']');

        return os;
    }

}; // end of specialization

/*
 * std::array<T, N>, T != char
 */
template<typename T>
struct json_io_traits<T, util::enable_if_t<(util::is_std_array<T>::value && ! util::is_std_char_array<T>::value)> >
{
        using value_type                    = typename T::value_type;
        static constexpr int32_t sz ()      { return util::array_size<T>::value; }

        using traits        = json_io_traits<value_type>;

        static int32_t consume (JSON_token_vector const & data, int32_t const position, T & x)
        {
            assert_within_inclusive (sz (), data.size () - position);

            int32_t consumed { };
            for (int32_t i = 0; i < sz (); ++ i)
            {
                consumed += traits::consume (data, position + consumed, x [i]);
            }

            if (VR_UNLIKELY ((position + consumed >= static_cast<int32_t> (data.size ())) || (data [position + consumed].m_type != parse::JSON_token::eoa)))
                throw_x (parse_failure, "expected ']'");
            else // consume end-of-array
                ++ consumed;

            return consumed;
        }

        static std::ostream & emit (T const & x, std::ostream & os)
        {
            os.put ('[');
            for (int32_t i = 0; i < sz (); ++ i)
            {
                if (i) os.put (',');
                traits::emit (x [i], os);
            }
            os.put (']');

            return os;
        }

};  // end of specialization
//............................................................................
/*
 * meta-object
 */
template<typename T>
struct json_io_traits<T,
    util::enable_if_t<meta::is_meta_struct<T>::value> >
{
    static int32_t consume (JSON_token_vector const & data, int32_t const position, T & x)
    {
        using visitor       =  impl::object_consume_visitor<T>;

        visitor v { x, data, position };
        meta::apply_visitor<T> (std::ref (v));

        return v.consumed ();
    }

    static std::ostream & emit (T const & x, std::ostream & os)
    {
        using visitor       =  impl::object_emit_visitor<T>;

        os.put ('{');
        {
            meta::apply_visitor<T> (visitor { x, os });
        }
        os.put ('}');

        return os;
    }

}; // end of specialization
//............................................................................

class json_object_serializer final: noncopyable
{
    public: // ...............................................................

        // TODO options (minimize, etc)?

        template<typename S>
        static void unmarshall (util::str_range const & s, S & obj) // TODO more error handling
        {
            using traits    = json_io_traits<S>;

            JSON_token_vector tokens { };
            int32_t const token_count = parse::JSON_tokenize (s.m_start, s.m_size, tokens);

            int32_t const consumed = traits::consume (tokens, 0, obj);

            if (VR_UNLIKELY (token_count != consumed))
                throw_x (parse_failure, "consumed only " + string_cast (consumed) + " JSON token(s) out of expected " + string_cast (token_count));
        }

        template<typename S>
        static void unmarshall (std::string const & s, S & obj)
        {
            unmarshall (util::str_range { s.data (), static_cast<util::str_range::size_type> (s.size ()) }, obj);
        }


        template<typename S>
        static std::ostream & marshall (S const & obj, std::ostream & os)
        {
            using traits    = json_io_traits<S>;

            return traits::emit (obj, os);
        }

}; // end of class

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
