#pragma once

#include "vr/io/links/socket_link.h"
#include "vr/sys/os.h"
#include "vr/util/logging.h"

#include <unistd.h>

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
//............................................................................
//............................................................................
namespace impl
{

extern net::socket_handle
open_TCP_socket (std::string const & host, std::string const & service, bool const connect, bool const disable_nagle);

/*
 * NOTE an rc convention different from UDP links:
 *
 * returns
 *      a positive byte count,
 *      0 on EAGAIN,
 *      -1 on EOF,
 *      throws on error
 */
extern VR_ASSUME_HOT capacity_t
tcp_recv_poll (int32_t const fd, addr_t const dst, capacity_t const len);

/*
 * returns a non-negative byte count, 0 on EAGAIN, throws on error
 */
extern VR_ASSUME_HOT capacity_t
tcp_send (int32_t const fd, addr_const_t const src, capacity_t const len);

} // end of 'impl'
//............................................................................
//............................................................................
// TODO parameterize time source (e.g. sim, sw/hw, etc)
/**
 * @see socket_link
 * @see link_base
 */
template<typename ... /* recv<_timestamp_, ...>, send<_timestamp_, ...>, _state_, ... */ASPECTs>
class TCP_link: public socket_link<TCP_link<ASPECTs ...>, ASPECTs ...>
{
    private: // ..............................................................

        using super         = socket_link<TCP_link<ASPECTs ...>, ASPECTs ...>;

    public: // ...............................................................

        TCP_link (std::string const & host, std::string const & service, recv_arg_map const & recv_args, send_arg_map const & send_args,
                  bool const disable_nagle = true) :
            super (recv_args, send_args, impl::open_TCP_socket (host, service, true, disable_nagle), join_as_name<'_'> (host, service))
        {
            vr_static_assert (super::link_mode () == (mode::recv | mode::send)); // catch ifc usage errors early at compile-time
        }

        TCP_link (net::socket_handle && sh, recv_arg_map const & recv_args, send_arg_map const & send_args) :
            super (recv_args, send_args, std::move (sh), "") // TODO use sh.peer_name()
        {
            vr_static_assert (super::link_mode () == (mode::recv | mode::send)); // catch ifc usage errors early at compile-time
        }


        TCP_link (std::string const & host, std::string const & service, recv_arg_map const & recv_args,
                  bool const disable_nagle = true) :
            super (recv_args, impl::open_TCP_socket (host, service, true, disable_nagle), join_as_name<'_'> (host, service))
        {
            vr_static_assert (super::link_mode () == mode::recv); // catch ifc usage errors early at compile-time
        }

        TCP_link (std::string const & host, std::string const & service, send_arg_map const & send_args,
                  bool const disable_nagle = true) :
            super (send_args, impl::open_TCP_socket (host, service, true, disable_nagle), join_as_name<'_'> (host, service))
        {
            vr_static_assert (super::link_mode () == mode::send); // catch ifc usage errors early at compile-time
        }

        ~TCP_link () VR_NOEXCEPT // calls 'close ()'
        {
            close ();
        }

        void close () VR_NOEXCEPT; // idempotent


        // link_base:

        std::pair<addr_const_t, capacity_t> recv_poll_impl ();
        capacity_t send_flush_impl (capacity_t const len);

    private: // ..............................................................

        using super::m_socket;

}; // end of class
//............................................................................

template<typename ... ASPECTs>
void
TCP_link<ASPECTs ...>::close () VR_NOEXCEPT
{
    if (super::has_state () && (super::state () == link_state::closed))
        return;

    super::close (); // chain

    if (super::has_state ()) super::state () = link_state::closed;
}
//............................................................................

template<typename ... ASPECTs>
std::pair<addr_const_t, capacity_t>
TCP_link<ASPECTs ...>::recv_poll_impl ()
{
    vr_static_assert (super::link_mode () & mode::recv); // catch ifc usage errors early at compile-time

    typename super::recv_impl & ifc = super::recv_ifc ();

    int32_t const fd = m_socket.fd ();
    capacity_t r { }; // total received in this invocation

    while (true) // try to drain 'fd', but only up until the point we'd block
    {
        assert_positive (ifc.w_window ());
        capacity_t const rc = impl::tcp_recv_poll (fd, ifc.w_position (), ifc.w_window ());

        if (rc > 0)
        {
            DLOG_trace2 << "  recv_poll rc: " << rc;

            r += rc;
            ifc.w_advance (rc);
        }
        else // rc <= 0
        {
            if (VR_UNLIKELY (rc < 0)) // EOF
            {
                if (r == 0) // guard necessary because may have accumulated some bytes in earlier loop iterations
                {
                    if (super::has_state ())
                    {
                        super::state () = link_state::closed;
                    }

                    throw_x (eof_exception, "recv_poll(): EOF detected"); // TODO add link parms into the message
                }
            }

            // [EAGAIN or EOF]

            break;
        }
    }

    if (super::has_ts_last_recv ()) // track the latest non-zero read
    {
        if (r > 0) super::ts_last_recv () = sys::realtime_utc ();
    }

    if (r > 0) DLOG_trace1 << "recv_poll(): exit (read " << r << ')';
    return { ifc.r_position (), ifc.size () }; // note: this returns totals (since last 'recv_flush()')
}
//............................................................................

template<typename ... ASPECTs>
capacity_t
TCP_link<ASPECTs ...>::send_flush_impl (capacity_t const len)
{
    vr_static_assert (super::link_mode () & mode::send); // catch ifc usage errors early at compile-time

    DLOG_trace1 << "send_flush(" << len << "): entry";
    assert_positive (len); // 'link_base' ensures

    typename super::send_impl & ifc = super::send_ifc ();

    int32_t const fd = m_socket.fd ();

    // TODO check in rt whether it's worth looping here or the 2nd send() tends to be EAGAIN

    capacity_t r { };
    while (r < len) // try to send all of 'len', but only up until the point we'd block
    {
        int32_t const rc = impl::tcp_send (fd, ifc.r_position (), len - r);

        if (rc > 0)
        {
            DLOG_trace2 << "  send_flush rc: " << rc;
            assert_le (rc, ifc.size ()); // design invariant

            r += rc;
            ifc.r_advance (rc); // can't do a single step on exit because a possible throw
        }
        else
        {
            // [EAGAIN]

            break;
        }
    }

    if (super::has_ts_last_send ()) // track the latest noz-zero write
    {
        if (r > 0) super::ts_last_send () = sys::realtime_utc ();
    }

    DLOG_trace1 << "send_flush(" << len << "): exit (wrote " << r << ')';
    return r;
}

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
