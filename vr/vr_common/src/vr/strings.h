#pragma once

#include "vr/meta/tags.h"
#include "vr/strings_fwd.h"
#include "vr/util/str_const.h"
#include "vr/util/type_traits.h"

#include <boost/function.hpp>
#include <boost/regex.hpp>

#include <algorithm>
#include <cstring>
#include <iterator>
#include <sstream>
#include <utility>

//----------------------------------------------------------------------------
namespace vr
{
//............................................................................

using str_const                     = util::str_const;

//............................................................................

template<std::size_t N>
constexpr uint64_t fw_cstr (char const (& str) [N])
{
    return str_const { str }.to_fw_string8 ();
}
//............................................................................

template<char QUOTE = '"'>
std::string
quote_string (string_literal_t const s)
{
    auto const s_len = std::strlen (s);

    std::string r (s_len + 2, QUOTE);
    std::memcpy (& r [1], s, s_len);

    return r;
}

template<char QUOTE = '"'>
std::string
quote_string (std::string const & s)
{
    return (QUOTE + s + QUOTE);
}

template<char QUOTE = '"', std::size_t N>
std::string
quote_string (std::array<char, N> const & a)
{
    auto const a_len = ::strnlen (a.data (), a.size ());

    std::string r (a_len + 2, QUOTE);
    std::memcpy (& r [1], a.data (), a_len);

    return r;
}
//............................................................................
//............................................................................
namespace impl
{
/*
 * note: counterintuitively, 'to_string()' seems slower than 'stringstream':
 * - http://stackoverflow.com/a/19429923
 */
//............................................................................

template<typename T, typename = void>
struct string_cast_impl
{
    static VR_NORETURN std::string evaluate (T const & x)
    {
        static_assert (util::lazy_false<T>::value, "'T' is not streamable");
    }

}; // end of master


template<typename T>
struct string_cast_impl<T,
    typename std::enable_if<util::is_std_string<T>::value>::type>
{
    static VR_FORCEINLINE const std::string & evaluate (std::string const & s)
    {
        return s;
    }

}; // end of specialization

template<typename T>
struct string_cast_impl<T,
    typename std::enable_if<util::is_std_char_array<T>::value>::type>
{
    static VR_FORCEINLINE std::string evaluate (T const & a)
    {
        auto const a_len = ::strnlen (a.data (), a.size ());

        return { a.data (), a_len };
    }

}; // end of specialization


template<typename T> // finally, use operator<< if available
struct string_cast_impl<T,
    typename std::enable_if<(! (util::is_std_string<T>::value || util::is_std_char_array<T>::value)
                             && util::is_streamable<T, std::stringstream>::value)>::type>
{
    static std::string evaluate (T const & x)
    {
        std::stringstream s;
        s << x;

        return s.str ();
    }

}; // end of specialization
//............................................................................


template<typename T, typename = void>
struct hex_string_cast_impl
{
    static VR_NORETURN std::string evaluate (T const & x)
    {
        static_assert (util::lazy_false<T>::value, "can't represent 'T' as hex string");
    }

}; // end of master


template<typename T>
struct hex_string_cast_impl<T,
    typename std::enable_if<std::is_integral<T>::value>::type>
{
    static VR_FORCEINLINE std::string evaluate (T const & x)
    {
        std::stringstream s;
        s << "0x" << std::hex << x;

        return s.str ();
    }

}; // end of specialization

template<typename T>
struct hex_string_cast_impl<T,
    typename std::enable_if<std::is_pointer<T>::value>::type>
{
    static VR_FORCEINLINE std::string evaluate (T const x)
    {
        std::stringstream s;
        s << static_cast<addr_const_t> (x);

        return s.str ();
    }

}; // end of specialization

} // end of 'impl'
//............................................................................
//............................................................................
/**
 * @return a string representation of 'x' as would be produced by 'std::ostream << x'
 *
 * @note as a special case, an 'std::string' will be passed through by reference
 */
template<typename T>
auto
string_cast (T const & x) -> decltype (impl::string_cast_impl<T>::evaluate (x))
{
    return impl::string_cast_impl<T>::evaluate (x);
}

template<typename T>
std::string
hex_string_cast (T const & x)
{
    return impl::hex_string_cast_impl<T>::evaluate (x);
}
//............................................................................
/*
 * a helper stream manipulator
 */
struct hexdump final
{
    hexdump (addr_const_t const data, int32_t const size) :
        m_data { data },
        m_size { size }
    {
    }

    friend std::ostream & operator<< (std::ostream & os, hexdump const & obj) VR_NOEXCEPT_IF (VR_RELEASE);

    addr_const_t const m_data;
    int32_t const m_size;

}; // end of class
//............................................................................
//............................................................................
namespace impl
{

template<typename T, typename = void>
struct print_impl
{
    static std::string evaluate (T const & x)
    {
        return string_cast (x); // default to 'string_cast' (which will likely require 'T' to be streamable)
    }

}; // end of master


template<typename T>
struct print_impl<T,
    typename util::enable_if_is_same<string_literal_t, T>::type> // specialize for C-style strings
{
    static std::string evaluate (string_literal_t const s)
    {
        return quote_string<'"'> (s); // TODO escape 's', for consistency?
    }

}; // end of specialization

template<typename T>
struct print_impl<T,
    typename util::enable_if_is_same<std::string, T>::type> // specialized for 'std::string'
{
    static std::string evaluate (std::string const & s)
    {
        return quote_string<'"'> (s); // TODO escape 's', for consistency?
    }

}; // end of specialization

template<typename T>
struct print_impl<T,
    typename std::enable_if<util::is_std_char_array<T>::value>::type> // specialized for 'std::array<char, N>'
{
    static std::string evaluate (T const & a)
    {
        return quote_string<'"'> (a); // TODO escape 'a', for consistency?
    }

}; // end of specialization


template<typename T>
struct print_impl<T,
    typename util::enable_if_is_same<char, T>::type>
{
    static std::string evaluate (char const & c)
    {
        std::string r { "'?'" };
        r [1] = c;

        return r;
    }

}; // end of specialization

template<typename T>
struct print_impl<T,
    typename util::enable_if_is_same<int8_t, T>::type>
{
    static std::string evaluate (int8_t const & x)
    {
        return print_impl<int32_t>::evaluate (x); // cast to 'int32_t'
    }

}; // end of specialization

template<typename T>
struct print_impl<T,
    typename util::enable_if_is_same<uint8_t, T>::type>
{
    static std::string evaluate (uint8_t const & x)
    {
        return print_impl<uint32_t>::evaluate (x); // cast to 'uint32_t'
    }

}; // end of specialization


template<typename T> // if '__print__ (T)' works, use that
struct print_impl<T,
    typename std::enable_if<util::has_print<T>::value>::type>
{
    static std::string evaluate (T const & x)
    {
        return __print__ (x);
    }

}; // end of specialization

// std::pair:

template<typename T>
struct print_impl<T,
    typename std::enable_if<util::is_pair<T>::value>::type>
{
    static std::string evaluate (T const & p)
    {
        std::stringstream s { };

        s << '{' << print (p.first) << ':' << print (p.second) << '}';

        return s.str ();
    }

}; // end of specialization

// containers:

template<typename C>
struct print_impl<C, // otherwise check if it's iterable (but not 'std::string' or 'std::array<char, N>')
    typename std::enable_if<(! util::has_print<C>::value && util::is_iterable<C>::value && ! (util::is_std_string<C>::value || util::is_std_char_array<C>::value)
                             && // recursion guard
        ! std::is_same<typename std::decay<decltype (* std::begin (std::declval<C> ()))>::type, typename std::decay<C>::type>::value)>::type>
{
    static std::string evaluate (C const & c)
    {
        std::stringstream s { };
        s << '{';
        {
            // TODO limit (emit "...") output length

            bool first = true;
            for (auto i = std::begin (c), i_limit = std::end (c); i != i_limit; ++ i, first = false)
            {
                if (! first) s << ", ";
                s << print (* i); // recurse
            }
        }
        s << '}';

        return s.str ();
    }

}; // end of specialization
//............................................................................

template<std::size_t N>
std::string
print_str_literal (char const (& s) [N]) // for C-style strings
{
    std::string r (N + 1, '"');
    __builtin_memcpy (& r [1], s, N - 1); // TODO escape 's', for consistency

    return r;
}

template<typename T, std::size_t N>
std::string
print_array (T const (& a) [N])
{
    std::stringstream s { };
    s << '{';
    {
        for (std::size_t i = 0; i != N; ++ i)
        {
            if (i) s << ", ";
            s << print (a [i]); // recurse
        }
    }
    s << '}';

    return s.str ();
}
//............................................................................

template<char SEP, typename ... ARGs>
struct join_as_name_impl; // master


template<char SEP> // recursion ends
struct join_as_name_impl<SEP>
{
    static void evaluate (std::ostream &, bool const)
    {
    }

}; // end of specialization

template<char SEP, typename T, typename ... ARGs>
struct join_as_name_impl<SEP, T, ARGs ...>
{
    static void evaluate (std::ostream & out, bool not_first, T && x, ARGs && ... args)
    {
        auto const x_str = string_cast (x);

        if (! x_str.empty ())
        {
            if (not_first) out << SEP;
            out << x_str;

            not_first = true;
        }

        join_as_name_impl<SEP, ARGs ...>::evaluate (out, not_first, std::forward<ARGs> (args) ...);
    }

}; // end of specialization

} // end of 'impl'
//............................................................................
//............................................................................
/**
 * @return a "human-friendly" stringified representation of 'x' (whatever that means)
 */
template<typename T>
std::string
print (T const & x)
{
    return impl::print_impl<T>::evaluate (x);
}

/**
 * @return a "human-friendly" stringified representation of 's' (whatever that means)
 */
template<std::size_t N> // overload for string literals
std::string
print (char const (& s) [N])
{
    return impl::print_str_literal<N> (s);
}

template<typename T, std::size_t N> // overload for raw arrays
std::string
print (T const (& a) [N])
{
    return impl::print_array<T, N> (a);
}
//............................................................................

template<typename TAG, bool QUOTE = false>
std::string
print_tag ()
{
    std::stringstream s { };

    meta::emit_tag_name<TAG, QUOTE> (s);

    return s.str ();
}
//............................................................................
/**
 * @return a string that is a SEP-separated list of 'args' as stringized by 'string_cast'
 *         (TODO with any resulting empty strings elided)
 *
 */
template<char SEP = '.', typename ... ARGs>
std::string
join_as_name (ARGs && ... args) // TODO shortcut optimization for single arg
{
    std::stringstream os;
    {
        impl::join_as_name_impl<SEP, ARGs ...>::evaluate (os, false, std::forward<ARGs> (args) ...);
    }
    return os.str ();
}
//............................................................................

using name_filter           = boost::function<bool (std::string const &)>;

//............................................................................

class regex_name_filter final
{
    public: // ...............................................................

        regex_name_filter (std::string const & regex);

        // name_filter:

        VR_ASSUME_HOT bool operator() (std::string const & s) const
        {
            return boost::regex_match (s, m_re);
        }

    private: // ..............................................................

        boost::regex const m_re;

}; // end of class
//............................................................................

class set_name_filter final
{
        using set_type              = boost::unordered_set<std::string>;

    public: // ...............................................................

        template<typename C>
        set_name_filter (C const & names)
        {
            // TODO warn about dups

            std::copy (std::begin (names), std::end (names), std::insert_iterator<set_type> { m_names, m_names.begin () });
        }

        template<typename C, typename _ = C>
        set_name_filter (C && names, typename std::enable_if<(/* don't steal from lvalues: */std::is_rvalue_reference<decltype (names)>::value)>::type * = { })
        {
            // TODO warn about dups

            std::move (std::begin (names), std::end (names), std::insert_iterator<set_type> { m_names, m_names.begin () }); // last reference to 'names'
        }

        set_name_filter (set_name_filter const & rhs) = default;
        set_name_filter (set_name_filter && rhs) = default;

        // name_filter:

        VR_ASSUME_HOT bool operator() (std::string const & s) const
        {
            return m_names.count (s);
        }

    private: // ..............................................................

        boost::unordered_set<std::string> m_names;

}; // end of class

} // end of namespace
//----------------------------------------------------------------------------
