
#include "vr/io/http/HTTP_source.h"

#include "vr/asserts.h"
#include "vr/io/exceptions.h" // http_exception
#include "vr/io/resizable_buffer.h"
#include "vr/io/uri.h"
#include "vr/sys/os.h"
#include "vr/util/format.h"
#include "vr/util/logging.h"
#include "vr/util/memory.h"
#include "vr/util/singleton.h"

#include <curl/curl.h>

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
//............................................................................
//............................................................................
namespace
{

#define vr_CHECKED_CURL_CALL(exp) \
    ({ \
       decltype (exp) rc; rc = (exp); \
       if (VR_UNLIKELY (rc != 0)) \
           { throw_x (io_exception, "(" #exp ") failed: rc = " + string_cast (rc)); }; /* TODO proper error handling */ \
    }) \
    /* */

#define vr_CHECKED_CURL_CALL_noexcept(exp) \
    ({ \
       decltype (exp) rc; rc = (exp); \
       if (VR_UNLIKELY (rc != 0)) \
           { LOG_error << "(" #exp ") failed, rc = " << rc; }; /* TODO proper error handling */ \
    }) \
    /* */


using session_ptr       = std::unique_ptr<::CURL, void (*) (::CURL *)>;
using multi_ptr         = std::unique_ptr<::CURLM, void (*) (::CURLM *)>;

//............................................................................

void
release_session_handle (::CURL * const handle)
{
    ::curl_easy_cleanup (handle); // void return
}

void
release_multi_handle (::CURLM * const multi_handle)
{
    vr_CHECKED_CURL_CALL_noexcept (::curl_multi_cleanup (multi_handle));
}
//............................................................................

struct curl_setup
{
    VR_ENUM (resolver,
        (
            synchronous,
            c_ares,
            threaded
        ),
        printable

    ); // end of nested enum

    curl_setup ()
    {
        if (VR_UNLIKELY (::curl_global_init (CURL_GLOBAL_DEFAULT)))
            LOG_error << "could not initialize libcurl";
        else
        {
            m_vi = ::curl_version_info (CURLVERSION_NOW);

            std::stringstream libidn_info;
            if (m_vi->age >= 2) libidn_info << ", libidn: "  << m_vi->libidn;

            LOG_trace1 << "initialized libcurl v" << m_vi->version << ", features " << hex_string_cast (m_vi->features)
                       << " ("
                          "resolver: " << resolver_type (* m_vi) << ", SSL: " << m_vi->ssl_version << libidn_info.str ()
                       << ')';

            m_user_agent += m_vi->version;
        }
    }

    ~curl_setup ()
    {
        if (m_vi)
        {
            m_vi = nullptr;

            LOG_trace1 << "cleaning up libcurl ...";
            ::curl_global_cleanup ();
        }
    }

    // ACCESSORs:

    std::string const & user_agent () const
    {
        return m_user_agent;
    }

    // MUTATORs:

    session_ptr create_session ()
    {
        if (VR_UNLIKELY (m_vi == nullptr))
            throw_x (io_exception, "libcurl failed to initialize"); // should this be non-throwing?

        return { ::curl_easy_init (), release_session_handle };
    }

    multi_ptr create_multi ()
    {
        if (VR_UNLIKELY (m_vi == nullptr))
            throw_x (io_exception, "libcurl failed to initialize"); // should this be non-throwing?

        return { ::curl_multi_init (), release_multi_handle };
    }


    private:

        static resolver::enum_t resolver_type (::curl_version_info_data const & vi)
        {
          if (! (vi.features & CURL_VERSION_ASYNCHDNS))
            return resolver::synchronous;
          else if (! vi.age || vi.ares_num)
            return resolver::c_ares;
          else
            return resolver::threaded;
        }


        ::curl_version_info_data const * m_vi { }; // also used as a guard for global init success
        std::string m_user_agent { "curl/" };

}; // end of class

} // end of anonymous
//............................................................................
//............................................................................
/*
 * note: this is a blocking 'Device' that is not 'Direct' (the latter due to libcurl API limitations)
 */
struct HTTP_source::pimpl final
{
    using size_type         = resizable_buffer::size_type;

    vr_static_assert (std::is_signed<size_type>::value);


    pimpl (uri const & url, arg_map const & parms) :
        m_url { url },
        m_retry_max { parms.get<int32_t> ("retry_max", 5) },
        m_retry_delay { parms.get<timestamp_t> ("retry_delay", 1500000000L) },
        m_headers (parms.get<string_vector> ("headers", { }))
    {
        LOG_trace1 << "url: " << print (m_url)
                   << ", retry max: " << m_retry_max << ", retry delay: " << m_retry_delay << " ns"
                   ", " << m_headers.size () << " custom header(s)";

        if (! m_headers.empty () && LOG_trace_enabled (2))
        {
            std::stringstream ss;
            ss << "headers:\n";

            for (std::string const & h : m_headers)
            {
                ss << "  [" << h << "]\n";
            }
            LOG_trace (2) << ss.str ();
        }
    }

    ~pimpl ()
    {
        close ();
        LOG_trace2 << this << ": final read buffer capacity: " << m_read_buf.capacity ();
    }


    /*
     * invariant: either 'm_multi' and 'm_session' are both null
     *            or
     *            they are both non-null and 'm_session' is NOT added to 'm_multi'
     */
    ::CURL * acquire_session_handles ()
    {
        ::CURL * session { };

        if (! m_session) // acquire new
        {
            check_condition (! m_multi);

            curl_setup & curl_singleton = util::singleton<curl_setup>::instance ();

            m_multi = curl_singleton.create_multi ();
            m_session = curl_singleton.create_session ();

            session = m_session.get ();

            LOG_trace2 << "  created session handle " << static_cast<addr_t> (session) << ", multi handle " << static_cast<addr_t> (m_multi.get ());
        }
        else // reset and reuse
        {
            check_condition (!! m_session & !! m_multi, !! m_session, !! m_multi);

            session = m_session.get ();

            LOG_trace2 << "  re-using session handle " << static_cast<addr_t> (session) << ", multi handle " << static_cast<addr_t> (m_multi.get ());

            // this will keep: live connections, DNS and cookie caches, etc

            ::curl_easy_reset (session);
        }
        assert_nonnull (session);

        return session;
    }

    void configure_session_handle ()
    {
        auto const session = m_session.get ();
        assert_nonnull (session);

        curl_setup & curl_ifc = util::singleton<curl_setup>::instance ();

        if (is_verbose ()) ::curl_easy_setopt (session, CURLOPT_VERBOSE, 1L); // TODO consider CURLOPT_DEBUGFUNCTION as well

        // [note: CURLOPT_TCP_NODELAY is set by default]

        // default choice is unsafe for multi-threaded libcurl usage:
        // [see also:
        //   https://stackoverflow.com/questions/21887264/why-libcurl-needs-curlopt-nosignal-option-and-what-are-side-effects-when-it-is,
        //   https://github.com/curl/curl/issues/383
        // ]
        vr_CHECKED_CURL_CALL (::curl_easy_setopt (session, CURLOPT_NOSIGNAL, 1L));

#   if LIBCURL_VERSION_NUM >= 0x072600 // don't use the timeout option for versions prior to 7.38 (https://github.com/sdkman/sdkman-cli/pull/364)
        vr_CHECKED_CURL_CALL (::curl_easy_setopt (session, CURLOPT_CONNECTTIMEOUT, 12L));
#   endif

        ::curl_easy_setopt (session, CURLOPT_DNS_USE_GLOBAL_CACHE, 0L); // note: this is the default already
        vr_CHECKED_CURL_CALL (::curl_easy_setopt (session, CURLOPT_DNS_CACHE_TIMEOUT, -1L)); // cache DNS entries forever

        vr_CHECKED_CURL_CALL (::curl_easy_setopt (session, CURLOPT_ERRORBUFFER, m_error_buf.get ()));
        vr_CHECKED_CURL_CALL (::curl_easy_setopt (session, CURLOPT_FAILONERROR, 1L)); // treat HTTP 4xx codes as I/O failures

        vr_CHECKED_CURL_CALL (::curl_easy_setopt (session, CURLOPT_USERAGENT, curl_ifc.user_agent ().c_str ()));

        vr_CHECKED_CURL_CALL (::curl_easy_setopt (session, CURLOPT_WRITEDATA, this));
        vr_CHECKED_CURL_CALL (::curl_easy_setopt (session, CURLOPT_WRITEFUNCTION, write_callback));

        if (! m_headers.empty ())
        {
            vr_CHECKED_CURL_CALL (::curl_easy_setopt (session, CURLOPT_HTTPHEADER, acquire_header_list ()));
        }

        if (m_total_received) // a retry, configure a partial read
        {
            char range [2 + std::numeric_limits<uint64_t>::digits10];

            auto const len = util::print_decimal_nonnegative (m_total_received, range);
            range [len] = '-';
            range [len + 1] = '\0';

            LOG_trace1 << "  partial read range: '" << range << '\'';

            vr_CHECKED_CURL_CALL (::curl_easy_setopt (session, CURLOPT_RANGE, range)); // docs say we can discard the value string
        }

        vr_CHECKED_CURL_CALL (::curl_easy_setopt (session, CURLOPT_URL, m_url.native ().c_str ()));
    }

    /*
     * this may be invoked multiple times, if retries are attempted
     */
    void open ()
    {
        LOG_trace1 << this << ": opening [" << m_url << "] ...";

        m_error_buf [0] = 0; // clear

        auto const session = acquire_session_handles (); // set and return 'm_session'

        configure_session_handle ();

        assert_condition (!! m_session);
        assert_condition (!! m_multi);

        vr_CHECKED_CURL_CALL (::curl_multi_add_handle (m_multi.get (), session));

        m_is_open = true;
    }

    // bio::device:

    void close () // 'closable_tag' comes with the convenience class the source derives from
    {
        bool const is_open = m_is_open;
        if (is_open)
        {
            m_is_open = false;

            LOG_trace1 << this << ": closing, received " << m_total_received << " byte(s) total";

            assert_condition (!! m_session);
            assert_condition (!! m_multi);

            vr_CHECKED_CURL_CALL_noexcept (::curl_multi_remove_handle (m_multi.get (), m_session.get ()));
        }

        release_header_list (); // not guarded by 'opened' becaus may have been allocated prior to subsequent errors
    }

    VR_FORCEINLINE std::streamsize read (char * const dst, std::streamsize const n)
    {
        LOG_trace1 << "read(" << static_cast<addr_t> (dst) << ", " << n << "): entry";

        size_type n_transferred { };

 retry: while (true) // implement blocking 'Device' behavior
        {
            // drain 'm_read_buf' first and update 'm_read_buf_size', 'm_read_buf_read' accordingly):
            {
                size_type read_buf_size = m_read_buf_size;

                if (read_buf_size) // we have data still buffered in 'm_read_buf'
                {
                    size_type read_buf_read = m_read_buf_read;

                    n_transferred = std::min<size_type> (n, read_buf_size - read_buf_read);
                    DLOG_trace1 << "  have " << (read_buf_size - read_buf_read) << " buffered byte(s), transferring " << n_transferred;

                    std::memcpy (dst, m_read_buf.data () + read_buf_read, n_transferred);

                    read_buf_read += n_transferred;

                    if (read_buf_read == read_buf_size) // 'm_read_buf' fully drained
                        m_read_buf_size = m_read_buf_read = 0;
                    else // 'n' limited us from draining fully, we can return at this point ('dst' full)
                    {
                        assert_eq (n_transferred, n);
                        m_read_buf_read = read_buf_read;

                        LOG_trace1 << "read(" << static_cast<addr_t> (dst) << ", " << n << "): exit, returning " << n_transferred;
                        return n_transferred;
                    }
                }
            }

            // [if we're here, 'm_read_buf' is empty and (n_transferred <= n)]

            assert_le (n_transferred, n);

            if (VR_UNLIKELY (! m_is_open)) // this lazy open can be done earlier, but this way we avoid a branch in buffered data case
            {
                open ();
            }

            auto const multi = m_multi.get ();

            // if we're here, 'm_read_buf' is empty and we *may* need to try receiving more data:
            //
            // [note that it could be that 'n_transferred' == 'n' already, in which case all of
            // new callback data will go into 'm_read_buf']

            assert_le (n_transferred, n);

            // update context for 'write_callback()':
            // [note that the current impl assumes that 'multi_perform()' will result in at most one callback]
            {
                m_callback_ctx.m_dst = dst + n_transferred;
                m_callback_ctx.m_dst_capacity = (n - n_transferred);
                m_callback_ctx.m_transferred = 0;
            }

            int32_t running { };
            vr_CHECKED_CURL_CALL (::curl_multi_perform (multi, & running)); // updates 'm_read_buf_size', 'm_wcallback_transferred'

            n_transferred += m_callback_ctx.m_transferred;

            if (n_transferred == n)
            {
                LOG_trace1 << "read(" << static_cast<addr_t> (dst) << ", " << n << "): exit, returning " << n_transferred;
                return n_transferred;
            }
            else // n_transferred < n
            {
                if (VR_UNLIKELY (! running)) // EOF (with or without errors)
                {
                    {
                        int32_t msg_queue_size { };
                        for (::CURLMsg * msg; (msg = ::curl_multi_info_read (multi, & msg_queue_size)) != nullptr; )
                        {
                            assert_eq (msg->msg, CURLMSG_DONE); // otherwise would still be 'running'

                            auto const rc = msg->data.result;
                            if (VR_UNLIKELY (rc != CURLE_OK)) // something went wrong and we have libcurl's error code at least
                            {
                                // obtain and log error details even if we're going to retry:

                                string_literal_t const errmsg = libcurl_errmsg (rc);

                                int64_t http_rc { }; // this will remain zero if no server response code has been received
                                ::curl_easy_getinfo (msg->easy_handle, CURLINFO_RESPONSE_CODE, & http_rc);

                                bool fail { (++ m_retry > m_retry_max) }; // always increment 'm_retry'
                                {
                                    switch (rc)
                                    {
                                        // consession to GCE (error possibly occur on auth token refreshes?)
                                        // [see https://github.com/GoogleCloudPlatform/google-cloud-php/issues/405]
                                        case CURLE_COULDNT_RESOLVE_HOST:

                                        case CURLE_PARTIAL_FILE:
                                        case CURLE_HTTP_RETURNED_ERROR:
                                        case CURLE_RECV_ERROR:
                                            break;

                                        default:
                                             fail = true;

                                    } // end of switch

                                    fail |= (http_rc == 404);
                                }
                                if (fail) throw_x (http_exception,
                                                   errmsg + (" (libcurl rc " + string_cast (rc)) + ", received " + string_cast (m_total_received) + ", url " + print (m_url) + ')',
                                                   static_cast<int32_t> (http_rc));

                                // else log a warning, pause, and attempt to resume as a partial download:

                                LOG_warn << errmsg << " (libcurl rc " << rc << ", HTTP status " << http_rc << ", received " << m_total_received << ", url " << print (m_url) << "),\n"
                                            "retrying " << m_retry << '/' << m_retry_max << " ...";

                                close (); // will be-open()d on the next loop iteration

                                sys::long_sleep_for (m_retry_delay << m_retry);
                                goto retry; // break out of the inner-most for
                            }
                        }
                    }

                    if (! n_transferred) n_transferred = -1;

                    LOG_trace1 << "read(" << static_cast<addr_t> (dst) << ", " << n << "): exit, EOF, returning " << n_transferred;
                    return n_transferred;
                }

                // otherwise block waiting for more data:

                int32_t fds_waited_on { };
                vr_CHECKED_CURL_CALL (::curl_multi_wait (multi, nullptr, 0, 1000, & fds_waited_on));

                if (! fds_waited_on) // don't busy spin if libcurl hasn't opened the connection yet
                    sys::short_sleep_for (100 * _1_millisecond ()); // 100ms, libcurl-recommended value
            }
        }

        VR_ASSUME_UNREACHABLE ();
    }

    // libcurl:

    static VR_ASSUME_HOT std::size_t write_callback (char * const data, std::size_t const size, std::size_t const nmemb, void/* this */ * const ctx)
    {
        LOG_trace2 << "  write_callback(" << static_cast<addr_t> (data) << ", " << size << ", " << nmemb << ", " << ctx << "): entry";

        assert_nonnull (ctx);
        HTTP_source::pimpl & this_ = * static_cast<HTTP_source::pimpl *> (ctx);

        assert_zero (this_.m_read_buf_size); // nothing is buffered on entry (always drained first)
        assert_zero (this_.m_read_buf_read);
        assert_zero (this_.m_callback_ctx.m_transferred); // this is the first and only invocation since the last 'curl_multi_perform()'

        // transfer however much we can directry to 'dst' of the outer 'read()':

        size_type const n = size * nmemb;
        this_.m_total_received += n;

        if (n > 0) // reset 'm_retry' on each new successful read
        {
            this_.m_retry = 0;
        }

        size_type const n_cpy = std::min<size_type> (this_.m_callback_ctx.m_dst_capacity, n);
        DLOG_trace3 << "    received " << n << " byte(s), transferring " << n_cpy;

        std::memcpy (this_.m_callback_ctx.m_dst, data, n_cpy);

        // buffer the rest in 'm_read_buf':

        size_type const n_remaining = (n - n_cpy);
        if (n_remaining)
        {
            DLOG_trace3 << "    buffering " << n_remaining << " byte(s)";

            int8_t * const read_buf_data = this_.m_read_buf.ensure_capacity (n_remaining);
            std::memcpy (read_buf_data, data + n_cpy, n_remaining);
        }

        this_.m_read_buf_size = n_remaining; // communicate how much data has been buffered in 'm_read_buf'
        this_.m_callback_ctx.m_transferred = n_cpy; // communicate how much data has been transferred to 'm_read_dst'

        LOG_trace2 << "  write_callback(" << static_cast<addr_t> (data) << ", " << size << ", " << nmemb << ", " << ctx << "): exit, transferred " << n_cpy;
        return n; // always tell libcurl we've consumed all incoming data
    }

    // support methods:

    /*
     * allocate or retrieve the curl slist anchored in 'm_header_list'
     */
    ::curl_slist * acquire_header_list ()
    {
        ::curl_slist * header_list = m_header_list;
        if (header_list) return header_list;

        for (std::string const & header : m_headers)
        {
            header_list = ::curl_slist_append (header_list, header.c_str ());
        }

        return (m_header_list = header_list);
    }

    void release_header_list ()
    {
        ::curl_slist * const header_list = m_header_list;
        if (header_list)
        {
            m_header_list = nullptr;
            ::curl_slist_free_all (header_list); // void return
        }
    }

    /*
     * returns a pointer to either 'm_error_buf' or a libcurl string literal
     */
    VR_NOINLINE VR_ASSUME_COLD string_literal_t libcurl_errmsg (::CURLcode const rc)
    {
        assert_ne (rc, CURLE_OK); // caller's responsibility

        string_literal_t const msg = m_error_buf.get ();

        return (msg [0] ? msg : ::curl_easy_strerror (rc));
    }

    static bool is_verbose ()
    {
        return LOG_trace_enabled (3);
    }


    uri const m_url;
    int32_t m_retry { };
    int32_t const m_retry_max;
    timestamp_t const m_retry_delay; // first retry delay (doubles on each retry)
    string_vector const m_headers;
    multi_ptr m_multi { nullptr, release_multi_handle };
    session_ptr m_session { nullptr, release_session_handle };
    ::curl_slist * m_header_list { nullptr }; // [threads strings inside 'm_headers']
    size_type m_total_received { }; // running tally of received bytes, whether or not they've been delivered
    resizable_buffer m_read_buf;    // data that's been consumed from libcurl but not yet delivered to a read()'s 'dst'
    size_type m_read_buf_size { };  // size of data extent in 'm_read_buf' + also callback out-parm
    size_type m_read_buf_read { };  // range [0, 'm_read_buf_read']) of data in 'm_read_buf' has been delivered to 'read()' caller(s)
    struct
    {
        char *    m_dst { nullptr };    // in-parm
        size_type m_dst_capacity { };   // in-parm
        size_type m_transferred { };    // out-parm
    }
    m_callback_ctx;
    std::unique_ptr<char []> const m_error_buf { boost::make_unique_noinit<char []> (CURL_ERROR_SIZE) };
    bool m_is_open { false };

}; // end of nested class
//............................................................................
//............................................................................

HTTP_source::HTTP_source (uri const & url, arg_map const & parms) :
    m_impl { std::make_shared<pimpl> (url, parms) }
{
}

HTTP_source::~HTTP_source ()
{
    close ();
}
//............................................................................

void
HTTP_source::close ()
{
    if (m_impl)
    {
        m_impl.reset ();

        super::close (); // [chain]
    }
}
//............................................................................

std::streamsize
HTTP_source::optimal_buffer_size ()
{
    vr_static_assert (buffer_size_hint () == CURL_MAX_WRITE_SIZE);

    // this choice allows us to avoid allocating actual memory for 'm_read_buf':

    return CURL_MAX_WRITE_SIZE;
}
//............................................................................

std::streamsize
HTTP_source::read (char * const dst, std::streamsize const n)
{
    assert_condition (m_impl);

    return m_impl->read (dst, n);
}

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
