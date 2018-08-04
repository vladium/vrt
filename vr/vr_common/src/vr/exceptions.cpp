
#include "vr/exceptions.h"

#include "vr/util/backtrace.h"
#include "vr/util/backtrace/symbols.h"
#include "vr/util/format.h"
#include "vr/util/logging.h"
#include "vr/util/memory.h" // make_unique

//----------------------------------------------------------------------------
namespace vr
{
//............................................................................
//............................................................................
namespace
{

#define vr_TAB  "  "
#define vr_TAB2 "    "


constexpr char sep_0        = '[';
constexpr char sep_1        = ':';
constexpr char tok_2 []     = "] ";
constexpr char tok_3 []     = "(): ";

constexpr int32_t line_digits10     = 1 + std::numeric_limits<int32_t>::digits10;
constexpr int32_t fixed_alloc_size  = /* null-terminator */1 + line_digits10 + sizeof (sep_0) + sizeof (sep_1) + (length (tok_2) - 1) + (length (tok_3) - 1);

constexpr char tok_4 []     = "()";

} // end of anonymous
//............................................................................
//............................................................................

app_exception::context::context (int32_t const message_capacity) :
    m_message { boost::make_unique_noinit<char []> (message_capacity) }
{
}
//............................................................................
//............................................................................

string_literal_t
app_exception::what () const VR_NOEXCEPT
{
    return m_context->m_message.get ();
}
//............................................................................

app_exception::context::ptr
app_exception::capture (string_literal_t const file, const int32_t line, string_literal_t const func,
                        string_literal_t const msg, std::size_t const msg_len) VR_NOEXCEPT_IF (VR_RELEASE)
{
    assert_nonnull (msg);

    // TODO format backtrace (TODO eliminate common lines with cause(s))

    auto const file_len = std::strlen (file);
    auto const func_len = std::strlen (func);

    int32_t const capacity     = fixed_alloc_size + file_len + func_len + msg_len;
    DLOG_trace2 << "allocating " << capacity << " message byte(s) ...";

    context::ptr const r { context::create (capacity) };

    static const int32_t callstack_skip  = /* app_exception::capture */1 + /* util::capture_callstack */1;

    r->m_size = util::capture_callstack (callstack_skip, r->m_addr, trace_data::depth_limit);
    DLOG_trace2 << "captured backtrace of depth " << r->m_size;

    {
        char * p = r->m_message.get ();

        * p ++ = sep_0;

        std::memcpy (p, file, file_len);
        p += file_len;

        * p ++ = sep_1;

        p += util::print_decimal (line, p);

        __builtin_memcpy (p, tok_2, length (tok_2) - 1);
        p += length (tok_2) - 1;

        std::memcpy (p, func, func_len);
        p += func_len;

        __builtin_memcpy (p, tok_3, length (tok_3) - 1);
        p += length (tok_3) - 1;

        std::memcpy (p, msg, msg_len);
        p += msg_len;

        DLOG_trace2 << "formatted " << (p - r->m_message.get ()) << " message byte(s)";

        * p = 0;
    }

    return r;
}
//............................................................................

void
app_exception::print_visit (std::exception const & e, std::ostream & out, bool const show_trace, std::string const & prefix, int32_t const depth)
{
    // TODO _Nested_exception<> messes things up trying to display 'util::class_name (e)'

    std::string local_prefix;
    {
        std::stringstream tmp;

        tmp << prefix;
        for (int32_t t = 0; t < depth; ++ t) tmp << vr_TAB;

        local_prefix = tmp.str ();
    }
    out << local_prefix;

    if (depth) out << VR_EXC_CAUSE;

    {
        string_literal_t e_msg = e.what ();
        if ((e_msg != nullptr) && e_msg [0]) out << e_msg;
    }

    out << "\r\n";


    if (show_trace)
    {
        app_exception const * is_app_exception;
        if ((is_app_exception = dynamic_cast<app_exception const *> (& e)))
        {
            const trace_data & trace = (* is_app_exception->m_context);

            if (trace.m_size > 0)
            {
                util::backtrace::print_trace (trace, out, local_prefix + vr_TAB2);
            }
        }
    }

    try
    {
        std::rethrow_if_nested (e);
    }
    catch (std::exception const & cause)
    {
        print_visit (cause, out, show_trace, prefix, depth + 1);
    }
}
//............................................................................

void
app_exception::print_trace (trace_data const & trace, std::ostream & out, std::string const & prefix)
{
    assert_positive (trace.m_size); // caller guarantee

    for (int32_t d = 0; d < trace.m_size; ++ d)
    {
        out << prefix << trace.m_addr [d] << "\r\n";
    }
}
//............................................................................

exc_info::exc_info (std::exception const & e, bool const show_trace, std::string const & indent) VR_NOEXCEPT :
    m_e { e },
    m_indent { indent },
    m_show_trace { show_trace }
{
}
//............................................................................

std::ostream &
operator<< (std::ostream & os, exc_info const & obj) VR_NOEXCEPT
{
    print_exception (obj.m_e, os, obj.m_show_trace, obj.m_indent);
    return os;
}
//............................................................................

std::ostream &
operator<< (std::ostream & os, control_flow_exception const & obj) VR_NOEXCEPT
{
    return os << sep_0 << obj.m_file << sep_1 << obj.m_line << tok_2 << obj.m_func << tok_4;
}

} // end of namespace
//----------------------------------------------------------------------------
