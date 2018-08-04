#pragma once

#include "vr/io/links/socket_link.h"
#include "vr/sys/os.h"
#include "vr/util/logging.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
//............................................................................
//............................................................................
namespace impl
{

extern VR_ASSUME_COLD net::socket_handle
open_ucast_socket (std::string const & host, std::string const & service, const bool set_peer);

/*
 * returns a non-negative byte count, 0 on EAGAIN, throws on error
 */
extern VR_ASSUME_HOT capacity_t
ucast_recv_poll (net::socket_handle const & sh, addr_t const dst, capacity_t const len);

/*
 * returns a non-negative byte count, 0 on EAGAIN, throws on error
 */
extern VR_ASSUME_HOT capacity_t
ucast_send (net::socket_handle const & sh, addr_const_t const src, capacity_t const len);

} // end of 'impl'
//............................................................................
//............................................................................
// TODO parameterize time source (e.g. sim, sw/hw, etc)
/**
 *
 * @note currently, this API doesn't allow to bind() the underlying socket
 *
 * @see socket_link
 * @see link_base
 */
template<typename ... /* recv<_timestamp_, ...>, send<_timestamp_, ...>, _state_, ... */ASPECTs>
class UDP_ucast_link: public socket_link<UDP_ucast_link<ASPECTs ...>, ASPECTs ...>
{
    private: // ..............................................................

        using super         = socket_link<UDP_ucast_link<ASPECTs ...>, ASPECTs ...>;

    public: // ...............................................................

        UDP_ucast_link (std::string const & host, std::string const & service, recv_arg_map const & recv_args, send_arg_map const & send_args) :
            super (recv_args, send_args, impl::open_ucast_socket (host, service, true), join_as_name<'_'> (host, service))
        {
            vr_static_assert (super::link_mode () == (mode::recv | mode::send)); // catch ifc usage errors early at compile-time
        }

        UDP_ucast_link (std::string const & host, std::string const & service, recv_arg_map const & recv_args) :
            super (recv_args, impl::open_ucast_socket (host, service, true), join_as_name<'_'> (host, service))
        {
            vr_static_assert (super::link_mode () == mode::recv); // catch ifc usage errors early at compile-time
        }

        UDP_ucast_link (std::string const & host, std::string const & service, send_arg_map const & send_args) :
            super (send_args, impl::open_ucast_socket (host, service, true), join_as_name<'_'> (host, service))
        {
            vr_static_assert (super::link_mode () == mode::send); // catch ifc usage errors early at compile-time
        }


        ~UDP_ucast_link () VR_NOEXCEPT // calls 'close ()'
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
UDP_ucast_link<ASPECTs ...>::close () VR_NOEXCEPT
{
    if (super::has_state () && (super::state () == link_state::closed))
        return;

    super::close (); // chain

    if (super::has_state ()) super::state () = link_state::closed;
}
//............................................................................

template<typename ... ASPECTs>
std::pair<addr_const_t, capacity_t>
UDP_ucast_link<ASPECTs ...>::recv_poll_impl ()
{
    vr_static_assert (super::link_mode () & mode::recv); // catch ifc usage errors early at compile-time

    typename super::recv_impl & ifc = super::recv_ifc ();

    capacity_t const rc = impl::ucast_recv_poll (m_socket, ifc.w_position (), ifc.w_window ()); // throws on an error other than 'EAGAIN'

    if (rc > 0)
    {
        if (super::has_ts_last_recv ()) // track the latest non-zero read
        {
            super::ts_last_recv () = sys::realtime_utc ();
        }

        DLOG_trace2 << "  recv_poll rc: " << rc;
        ifc.w_advance (rc);
    }

    if (rc > 0) DLOG_trace1 << "recv_poll(): exit (read " << rc << ')'; // TODO not merging in the above block because that's changing to use hw ts
    return { ifc.r_position (), ifc.size () }; // notes: this returns totals (since last 'recv_flush()')
}
//............................................................................

template<typename ... ASPECTs>
capacity_t
UDP_ucast_link<ASPECTs ...>::send_flush_impl (capacity_t const len)
{
    vr_static_assert (super::link_mode () & mode::send); // catch ifc usage errors early at compile-time

    DLOG_trace1 << "send_flush(" << len << "): entry";
    assert_positive (len); // 'link_base' ensures

    typename super::send_impl & ifc = super::send_ifc ();

    capacity_t rc = impl::ucast_send (m_socket, ifc.r_position (), len);

    if (rc > 0)
    {
        if (super::has_ts_last_send ()) // track the latest noz-zero send
        {
            super::ts_last_send () = sys::realtime_utc ();
        }

        DLOG_trace2 << "  send_flush rc: " << rc;
        assert_le (rc, ifc.size ()); // design invariant

        ifc.r_advance (rc);
    }

    if (rc > 0) DLOG_trace1 << "send_flush(" << len << "): exit (wrote " << rc << ')'; // TODO not merging in the above block because that's changing to use hw ts
    return rc;
}

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
