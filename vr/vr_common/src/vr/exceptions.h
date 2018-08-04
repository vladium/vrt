#pragma once

#include "vr/macros.h" // VR_FILENAME
#include "vr/util/impl/constexpr_strings.h" // util::impl::path_stem() used by VR_FILENAME
#include "vr/util/shared_array.h"

#include <cstring> // std::strlen()
#include <exception>
#include <limits>
#include <memory>

//----------------------------------------------------------------------------
/*
 * exception throwing/chaining:
 */
#define throw_x(cls, msg, /* extra args */...)  throw cls { VR_FILENAME, __LINE__, __func__, (msg), __VA_ARGS__ }
#define chain_x(cls, msg, /* extra args */...)  std::throw_with_nested (cls { VR_FILENAME, __LINE__, __func__, (msg), __VA_ARGS__ })
/*
 * for 'control_flow_exception' and derivatives:
 */
#define throw_cfx(cls, /* extra args */...)     throw cls { VR_FILENAME, __LINE__, __func__, __VA_ARGS__ }


#define VR_EXC_LOC_ARGS     string_literal_t const file, int32_t const line, string_literal_t const func
#define VR_EXC_CAUSE        "CAUSE: "

//----------------------------------------------------------------------------
namespace vr
{
//............................................................................

struct trace_data: noncopyable
{
    static const int32_t depth_limit    = 14; // TODO runtime sizing ?

    addr_t m_addr [depth_limit];
    int32_t m_size { }; // this choice of field ordering makes for a more compact 'app_exception::context' if used as 1st base
                        // TODO could also denote the end of 'm_addr' by the first nullptr entry
}; // end of class
//............................................................................
/**
 * equivalent to
 * @code
 *     app_exception::print (e, out, show_trace, indent);
 * @endcode
 */
inline void
print_exception (std::exception const & e, std::ostream & out, bool const show_trace = true, std::string const & indent = { });

//............................................................................
/**
 * root of the exception type hierarchy
 */
class app_exception: public std::exception // copyable (not merely movable) for strict standard compliance and std::throw_with_nested()
{
    public: // ...............................................................

        // all constructors force-inlined to avoid seeing them in stack traces:

        explicit VR_FORCEINLINE app_exception (VR_EXC_LOC_ARGS, string_literal_t const msg) VR_NOEXCEPT :
            m_context { capture (file, line, func, msg, std::strlen (msg)) }
        {
        }

        explicit VR_FORCEINLINE app_exception (VR_EXC_LOC_ARGS, std::string const & msg) VR_NOEXCEPT :
            m_context { capture (file, line, func, msg.c_str (), msg.size ()) }
        {
        }

        // std::exception:

        string_literal_t what () const VR_NOEXCEPT override;

        // custom extensions:

        /**
         * @note this will traverse the chain of nested exceptions starting with 'e', if any
         */
        static void print (std::exception const & e, std::ostream & out, bool const show_trace = true, std::string const & indent = { })
        {
            print_visit (e, out, show_trace, indent, 0);
        }

    protected: // ............................................................

        struct context final: public trace_data, public util::shared_object<context, thread::safe>
        {
            context (int32_t const message_capacity);

            std::unique_ptr<char []> const m_message;

        }; // end of nested class

        static_assert (sizeof (context) <= 128, "'context' too large"); // no technical limit, just want to minimize cache line hits

    private: // ..............................................................

        static VR_ASSUME_COLD VR_NOINLINE context::ptr capture (VR_EXC_LOC_ARGS, string_literal_t const msg, std::size_t const msg_len) VR_NOEXCEPT_IF (VR_RELEASE);

        static VR_ASSUME_COLD void print_visit (std::exception const & e, std::ostream & out, bool const show_trace, std::string const & prefix, int32_t const depth);
        static VR_ASSUME_COLD void print_trace (trace_data const & trace, std::ostream & out, std::string const & prefix);

        context::ptr const m_context { };

}; // end of class
//............................................................................
#define VR_DEFINE_EXCEPTION(cls, base) \
    class cls : public base \
    { \
        using super     = base; \
        \
        public: \
        \
            using super::super; /* inherit constructors */ \
        \
    } \
    /* */

    VR_DEFINE_EXCEPTION (alloc_failure, app_exception);
    VR_DEFINE_EXCEPTION (illegal_state, app_exception);
    VR_DEFINE_EXCEPTION (invalid_input, app_exception);
    VR_DEFINE_EXCEPTION (io_exception,  app_exception);
    VR_DEFINE_EXCEPTION (sys_exception, app_exception);

    VR_DEFINE_EXCEPTION (eof_exception,     io_exception);

    VR_DEFINE_EXCEPTION (check_failure,     invalid_input);
    VR_DEFINE_EXCEPTION (out_of_bounds,     invalid_input);
    VR_DEFINE_EXCEPTION (type_mismatch,     invalid_input);
    VR_DEFINE_EXCEPTION (parse_failure,     invalid_input);
    VR_DEFINE_EXCEPTION (capacity_limit,    invalid_input);

    VR_DEFINE_EXCEPTION (bad_address,       out_of_bounds);

//............................................................................
/**
 * manipulator-like wrapper for exceptions:
 *
 * @code
 *  std::cout << exc_info (e) << std::endl;
 * @endcode
 * @code
 *  LOG_error << exc_info (e);
 * @endcode
 */
class exc_info final: noncopyable
{
    public: // ...............................................................

        // note: all methods outlines by design, to reduce size of code emitted for catch blocks

        exc_info (std::exception const & e, bool const show_trace = true, std::string const & indent = { }) VR_NOEXCEPT;

        friend std::ostream & operator<< (std::ostream & os, exc_info const & obj) VR_NOEXCEPT;

    private: // ..............................................................

        std::exception const & m_e; // note: stored by ref
        std::string const m_indent;
        bool const m_show_trace;

}; // end of class
//............................................................................

inline void
print_exception (std::exception const & e, std::ostream & out, bool const show_trace, std::string const & indent)
{
    app_exception::print (e, out, show_trace, indent);
}
//............................................................................

/**
 * a special base exception class that does NOT derive from @ref app_exception
 * and hence does not incur the high cost of collecting detailed backtraces
 */
class control_flow_exception
{
    public: // ...............................................................

        // TODO use 'str_const' or something similar to constexpr this
        // (finish utils in "util/impl/constexpr_strings.h")

        control_flow_exception (VR_EXC_LOC_ARGS) :
            m_file { file },
            m_func { func },
            m_line { line }
        {
        }

        friend std::ostream & operator<< (std::ostream & os, control_flow_exception const & obj) VR_NOEXCEPT;

    private: // ..............................................................

        string_literal_t const m_file;
        string_literal_t const m_func;
        int32_t m_line;

}; // end of class
//............................................................................

} // end of namespace
//----------------------------------------------------------------------------
