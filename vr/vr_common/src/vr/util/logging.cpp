
#include "vr/util/logging.h"

#include "vr/config.h"
#include "vr/io/files.h"
#include "vr/io/streams.h"
#include "vr/sys/signals.h" // note: this inclusion triggers signal setup
#include "vr/util/datetime.h"
#include "vr/util/env.h"
#include "vr/util/singleton.h"

#include <iostream>

extern char const * __attribute__((weak)) app_version ();

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
namespace g = ::google;

#if !defined (VR_ENV_LOG_CONSOLE)
#   define VR_ENV_LOG_CONSOLE       VR_APP_NAME "_LOG_CONSOLE"
#endif

//............................................................................
//............................................................................
namespace
{

struct ostream_sink final: public g::LogSink // tee short versions of messages to a given std::ostream
{
    ostream_sink (fs::path const & out_file) : // empty means use 'std::cout'
        m_console_os { out_file.empty () ? nullptr : new io::fd_ostream<io::no_exceptions> { out_file } },
        m_out { m_console_os == nullptr ? std::cout : * m_console_os }
    {
    }

    // google::LogSink:

    VR_ASSUME_HOT void send (g::LogSeverity const severity,
                             string_literal_t const full_filename, string_literal_t const base_filename, const int32_t line,
                             ::tm const * const tm_time,
                             string_literal_t const message, std::size_t const message_len) override
    {
        m_out << '[' << base_filename << ':' << line << "] "; m_out.write (message, message_len); // 'message' not 0-terminated
        m_out.put ('\n');

        if (m_console_os) m_out.flush ();
    }

    std::unique_ptr<std::ostream> m_console_os { }; // must be constructed before 'm_out'
    std::ostream & m_out; // references 'm_console_os' or 'std::cout'

}; // end of class

struct glog_setup final: noncopyable
{
    VR_ASSUME_COLD glog_setup ()
    {
        m_start = pt::microsec_clock::local_time (); // note: not using 'sys::realtime_utc ()' to avoid a dep cycle

        // determine log root (parent dir for all log files):

        m_log_root = getenv<fs::path> (VR_ENV_LOG_ROOT, ".logs");

        if (m_log_root.has_parent_path ())
        {
            try
            {
                io::create_dirs (m_log_root);
            }
            catch (std::exception & e)
            {
                std::cerr << "failed to create log root, using current directory for logging: " << e.what () << std::endl;
                m_log_root = ".";
            }
            catch (...)
            {
                std::cerr << "failed to create log root, using current directory for logging" << std::endl;
                m_log_root = ".";
            }
        }

        // force glog to use our log root dir:

        for (int32_t severity = 0; severity < g::NUM_SEVERITIES; ++ severity)
        {
            g::SetLogDestination (severity, (m_log_root / (std::string { VR_APP_NAME "." } + g::LogSeverityNames [severity])).string ().c_str ());
            g::SetLogSymlink (severity, "last." VR_APP_NAME);
        }

        // if requested (default), install glog signal handler:

        if (getenv<std::string> (VR_ENV_SIG_HANDLER, "glog") == "glog")
        {
            g::InstallFailureSignalHandler ();
        }

        fs::path console_file = getenv<fs::path> (VR_ENV_LOG_CONSOLE);
        if (! console_file.empty ())
        {
            if (console_file.has_parent_path ())
            {
                try
                {
                    io::create_dirs (console_file.parent_path ());
                }
                catch (std::exception & e)
                {
                    std::cerr << "failed to create console file parent dir, using current directory: " << e.what () << std::endl;
                    console_file = ".";
                }
                catch (...)
                {
                    std::cerr << "failed to create console file parent dir, using current directory" << std::endl;
                    console_file = ".";
                }
            }
        }

        m_cout_appender = std::make_unique<ostream_sink> (console_file);

        g::AddLogSink (m_cout_appender.get ());


        g::InitGoogleLogging ("");
        m_initialized = true;

        if (! console_file.empty ()) std::cout << "console file [" << io::absolute_path (console_file).native () << ']' << std::endl;
        LOG_info << '[' << (app_version ? app_version () : "unknown") << "] start " << m_start << ", log root [" << io::absolute_path (m_log_root).native () << ']';
    }

    VR_ASSUME_COLD ~glog_setup ()
    {
        const bool initialized = m_initialized;
        m_initialized = false;

        if (initialized)
        {
            auto const stop = pt::microsec_clock::local_time (); // note: not using 'sys::realtime_utc ()' to avoid a dep cycle

            LOG_info << '[' << (app_version ? app_version () : "unknown") << "] DONE " << stop << ", elapsed: " << (stop - m_start);
            LOG_trace1 << "shutting down logging ...";

            g::ShutdownGoogleLogging ();
        }
    }


    const bool & is_initialized ()
    {
        return m_initialized;
    }


    fs::path m_log_root { };
    std::unique_ptr<ostream_sink> m_cout_appender { };
    util::ptime_t m_start;
    bool m_initialized { false };

}; // end of class

using glog_setup_singleton          = util::singleton<glog_setup>;

} // end of anonymous
//............................................................................
//............................................................................

bool
log_initialize ()
{
    return glog_setup_singleton::instance ().is_initialized ();
}

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
