
#include "vr/exceptions.h"

#include "vr/filesystem.h"
#include "vr/util/classes.h"
#include "vr/util/logging.h"

#include <boost/algorithm/string/predicate.hpp>

#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
//............................................................................

TEST (app_exception, size)
{
    LOG_info << "sizeof (std::exception) = " << sizeof (std::exception);
    LOG_info << "sizeof (app_exception) = " << sizeof (app_exception);
    LOG_info << "sizeof (alloc_failure) = " << sizeof (alloc_failure);
}
//............................................................................
//............................................................................
namespace
{

void
throw_io_exception (int32_t & /* out */line)
{
    line = __LINE__; throw_x (io_exception, "level.0");
}

void
throw_app_exception_with_cause (int32_t & /* out */line, int32_t & /* out */cause_line)
{
    try
    {
        throw_io_exception (cause_line);
    }
    catch (...)
    {
        line = __LINE__; chain_x (app_exception, "level.1");
    }
}

void
throw_deeply_nested (int32_t const depth)
{
    if (depth <= 0) throw_x (app_exception, "original cause");

    try
    {
        throw_deeply_nested (depth - 1); // recurse
    }
    catch (...)
    {
        chain_x (app_exception, "wraps " + std::to_string (depth)); // chain
    }
}
//............................................................................

const std::string g_filename { fs::path {__FILE__}.filename ().string () };

//............................................................................

void
check_nested_exception_message (int32_t const depth, std::string const & indent, string_vector const & lines)
{
    ASSERT_GE (signed_cast (lines.size ()), depth);

    int32_t l;

    for (l = 0; l < depth; ++ l)
    {
        const std::string & line = lines [l];

        // indenting:

        if (! indent.empty ()) { EXPECT_TRUE (boost::starts_with (line, indent)) << "line: [" << line << ']'; }

        if (l > 0)
        {
            // check VR_EXC_CAUSE presence:

            EXPECT_TRUE (line.find (VR_EXC_CAUSE) != std::string::npos) << "line: [" << line << ']';
        }

        // check message references to "wraps <depth>" and function name:

        EXPECT_TRUE (line.find ("wraps " + std::to_string (depth - l)) != std::string::npos) << "line: [" << line << ']';
        EXPECT_TRUE (line.find ("throw_deeply_nested") != std::string::npos) << "line: [" << line << ']';
    }
    {
        const std::string & line = lines [l];

        // indenting:

        if (! indent.empty ()) { EXPECT_TRUE (boost::starts_with (line, indent)) << "line: [" << line << ']'; }

        // check VR_EXC_CAUSE presence:

        EXPECT_TRUE (line.find (VR_EXC_CAUSE) != std::string::npos) << "line: [" << line << ']';

        // check message references to "original cause" and function name:

        EXPECT_TRUE (line.find ("original cause") != std::string::npos) << "line: [" << line << ']';
        EXPECT_TRUE (line.find ("throw_deeply_nested") != std::string::npos) << "line: [" << line << ']';
    }
}

void
check_nested_exception_message_and_trace (int32_t const depth, std::string const & indent, string_vector const & lines)
{
    int32_t const line_count = lines.size ();

    ASSERT_GE (line_count, depth);

    int32_t exc_count { }; // should end up as (depth + 1)

    for (int32_t l = 0; l < line_count; ++ l)
    {
        const std::string & line = lines [l];

        // identify start of a message+trace section by whether it contains VR_EXC_CAUSE or "wraps":

        if ((line.find (VR_EXC_CAUSE) != std::string::npos) || (line.find ("wraps") != std::string::npos))
        {
            // check the message line, it's identical to what we test in the previous testcase:
            {
                // indenting:

                if (! indent.empty ()) { EXPECT_TRUE (boost::starts_with (line, indent)) << "line: [" << line << ']'; }

                if (exc_count > 0)
                {
                    // check VR_EXC_CAUSE presence:

                    EXPECT_TRUE (line.find (VR_EXC_CAUSE) != std::string::npos) << "line: [" << line << ']';
                }

                // check message references to "wraps <depth>" and function name:

                if (exc_count < depth)
                    EXPECT_TRUE (line.find ("wraps " + std::to_string (depth - exc_count)) != std::string::npos) << "line: [" << line << ']';
                else
                    EXPECT_TRUE (line.find ("original cause") != std::string::npos) << "line: [" << line << ']';
                EXPECT_TRUE (line.find ("throw_deeply_nested") != std::string::npos) << "line: [" << line << ']';
            }

            // test that there is at least one more line (start of backtrace) and it references 'throw_deeply_nested()':

            ASSERT_GT (line_count, l);
            {
                const std::string & trace_start = lines [l + 1];
                EXPECT_TRUE (trace_start.find ("throw_deeply_nested") != std::string::npos) << "trace start: [" << trace_start << ']';
            }

            ++ exc_count;
        }
    }

    EXPECT_EQ (depth + 1, exc_count) << "failed to see the entire cause chain";
}

} // end of anonymous
//............................................................................
//............................................................................

TEST (exception, src_loc_info)
{
    int32_t line { };

    bool caught = false;
    try
    {
        throw_io_exception (line);
    }
    catch (std::exception const & e) // catch via supertype
    {
        caught = true;

        app_exception const * ae = dynamic_cast<app_exception const *> (& e);
        ASSERT_TRUE (ae != nullptr);

        // check exception loc info:

        LOG_trace1 << e.what ();
        std::string const msg { e.what () };

        EXPECT_TRUE (msg.find (g_filename) != std::string::npos);
        EXPECT_TRUE (msg.find (string_cast (line)) != std::string::npos);
        EXPECT_TRUE (msg.find ("throw_io_exception") != std::string::npos);
        EXPECT_TRUE (msg.find ("level.0") != std::string::npos);
    }
    ASSERT_TRUE (caught);
}
//............................................................................

TEST (exception, src_loc_info_nested)
{
    int32_t line { }, cause_line { };
    std::exception_ptr cause;

    {
        bool caught = false;
        try
        {
            throw_app_exception_with_cause (line, cause_line);
        }
        catch (app_exception const & ae) // catch via most derived type [note: std::throw_with_nested() still throws 'cls' type]
        {
            caught = true;

            // check exception loc info:

            LOG_trace1 << ae.what ();
            std::string const msg { ae.what () };

            EXPECT_TRUE (msg.find (g_filename) != std::string::npos);
            EXPECT_TRUE (msg.find (string_cast (line)) != std::string::npos);
            EXPECT_TRUE (msg.find ("throw_app_exception_with_cause") != std::string::npos);
            EXPECT_TRUE (msg.find ("level.1") != std::string::npos);

            // check chaining:

            std::nested_exception const * cause_ptr = dynamic_cast<std::nested_exception const *> (& ae);
            ASSERT_TRUE (cause_ptr != nullptr);
            cause = cause_ptr->nested_ptr ();
        }
        ASSERT_TRUE (caught);
    }
    ASSERT_TRUE (cause);
    {
        bool caught = false;
        try
        {
            std::rethrow_exception (cause);
        }
        catch (io_exception const & ioe) // catch via most derived type
        {
            caught = true;

            // check exception loc info:

            LOG_trace1 << ioe.what ();
            std::string const msg { ioe.what () };

            EXPECT_TRUE (msg.find (g_filename) != std::string::npos);
            EXPECT_TRUE (msg.find (string_cast (cause_line)) != std::string::npos);
            EXPECT_TRUE (msg.find ("throw_io_exception") != std::string::npos);
            EXPECT_TRUE (msg.find ("level.0") != std::string::npos);
        }
        ASSERT_TRUE (caught);
    }
}
//............................................................................

TEST (exception, print_exception_message_only)
{
    int32_t const depth         = 5;
    std::string const indent    = "*";

    std::stringstream out;
    try
    {
        throw_deeply_nested (depth);
    }
    catch (std::exception const & e)
    {
        print_exception (e, out, /* show_trace */false, indent);
    }

    LOG_trace1 << "printed exception:\n" << out.str ();

    string_vector lines;
    test::read_lines (out, lines);

    check_nested_exception_message (depth, indent, lines);
}

TEST (exception, DISABLED_print_exception_with_trace) // pending ASX-12
{
    int32_t const depth         = 3;
    std::string const indent    = "*";

    std::stringstream out;
    try
    {
        throw_deeply_nested (depth);
    }
    catch (std::exception const & e)
    {
        print_exception (e, out, /* show_trace */true, indent);
    }

    LOG_trace1 << "printed exception:\n" << out.str ();

    string_vector lines;
    test::read_lines (out, lines);

    check_nested_exception_message_and_trace (depth, indent, lines);
}
//............................................................................

TEST (exception, DISABLED_exc_info) // pending ASX-12
{
    int32_t const depth         = 4;
    std::string const indent    = "*";

    std::stringstream out;
    try
    {
        throw_deeply_nested (depth);
    }
    catch (std::exception const & e)
    {
        out << exc_info (e) << std::endl;
    }

    LOG_trace1 << "printed exception:\n" << out.str ();

    string_vector lines;
    test::read_lines (out, lines);

    // 'exc_info' defaults are to print trace, no indent:

    check_nested_exception_message_and_trace (depth, "", lines);
}
//............................................................................

TEST (control_flow_exception, print_message)
{
    std::stringstream out;
    try
    {
        throw_cfx (control_flow_exception);
    }
    catch (control_flow_exception const & cfe)
    {
        out << cfe;
    }

    std::string const msg = out.str ();
    LOG_trace1 << "printed exception:\n" << msg;

    EXPECT_FALSE (msg.empty ());
}

} // end of namespace
//----------------------------------------------------------------------------
