#pragma once

#include "vr/io/events/event_context.h"
#include "vr/io/links/socket_link.h"
#include "vr/io/net/IP_.h"
#include "vr/io/net/mcast_source.h"
#include "vr/io/net/UDP_.h"
#include "vr/io/net/utility.h" // make_group_range_filter
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

// 'type: SOCK_RAW or SOCK_DGRAM:

extern VR_ASSUME_COLD net::socket_handle
open_mcast_recv_link (std::string const & ifc_name, int32_t const type, net::ts_policy::enum_t const tsp);

extern VR_ASSUME_COLD net::socket_handle
open_mcast_send_link (std::string const & ifc_name, int32_t const type);

//............................................................................

extern VR_ASSUME_COLD net::socket_handle
join_mcast_sources (std::string const & ifc_name, std::vector<net::mcast_source> const & sources);

//............................................................................

/*
 * returns a non-negative byte count, 0 on EAGAIN, throws on error
 */
extern VR_ASSUME_HOT capacity_t
mcast_send (net::socket_handle const & sh, addr_const_t const src, capacity_t const len);

//............................................................................

template<bool ENABLED = false>
struct filter_impl
{
    filter_impl (arg_map const &, net::mcast_source const *)
    {
    }

    VR_FORCEINLINE bool drop (addr_t const, signed_size_t const) const
    {
        return false;
    }

}; // end of specialization

/*
 * filter for UDP (the fact that it will be ITCH multicast is ensured with host setup)
 */
template<>
struct filter_impl</* ENABLED */true>
{
    filter_impl (arg_map const & args, net::mcast_source const * ms) :
        m_proto_filter { args },
        m_ip_range_filter { net::make_group_range_filter (* ms) } // HACK
    {
    }

    using filter_ctx        = event_context<net::_filtered_>;
    using proto_filter      = net::IP_<net::UDP_<>>;

    VR_FORCEINLINE bool drop (addr_t const data, signed_size_t const data_len)
    {
        // HACK:
        {
            ::ip const * const ip_hdr = static_cast<::ip const *> (addr_plus (data, proto_filter::eth_hdr_len ()));
            if (! m_ip_range_filter (ip_hdr->ip_dst.s_addr)) // single-branch test
            {
                DLOG_trace1 << "HACK dropping packet: " << print (* ip_hdr);
                return true;
            }
        }

        filter_ctx ctx { };

        field<net::_filtered_> (ctx) = false;

        auto const ipf_rc = m_proto_filter.consume (ctx, data, data_len);

        bool const rc = ((ipf_rc < 0) | field<net::_filtered_> (ctx)); // note: guarding against shorter IP encapsulation than present in prod

        if (rc) DLOG_trace1 << "dropping packet: " << print (* static_cast<::ip const *> (addr_plus (data, proto_filter::eth_hdr_len ())));

        return rc;
    }

    proto_filter m_proto_filter;
    IP_range_filter const m_ip_range_filter; // HACK

}; // end of specialization
//............................................................................

template<typename ... ASPECTs>
struct filter_impl_for
{
    using type          = filter_impl<recv_is_filtered<ASPECTs...>::value>;

}; // end of metafunction
//............................................................................

template<mode::enum_t MODE>
struct mcast_group_socket_handle; // master


template<>
struct mcast_group_socket_handle<mode::recv>
{
    mcast_group_socket_handle (net::socket_handle && sh_group) :
        m_sh_group { std::move (sh_group) }
    {
    }

    void close () VR_NOEXCEPT // idempotent
    {
        m_sh_group.close ();
    }

    net::socket_handle m_sh_group;

}; // end of specialization

template<>
struct mcast_group_socket_handle<mode::send>
{
    mcast_group_socket_handle ()    = default;

    void close () VR_NOEXCEPT // no-op
    {
    }

}; // end of specialization
//............................................................................

template<typename ... ASPECTs>
struct group_socket_handle_for
{
    using type          = mcast_group_socket_handle<declared_mode<ASPECTs...>::value>;

}; // end of metafunction

} // end of 'impl'
//............................................................................
//............................................................................
// TODO parameterize time source (e.g. sim, sw/hw, etc)
/**
 * @note this is a RAW link, so for sending dest ports need to be set by the caller
 *       via the ::ip header, etc
 *
 * unlike other socket_link implementations, this can be either send<...> or recv<...>,
 * but not both at the same time
 *
 * @see socket_link
 * @see link_base
 */
template<typename ... /* (recv|send)<_timestamp_, _filter_, ...>, _state_, ... */ASPECTs> // note: '_filter_' for 'recv<>' only
class UDP_mcast_link: public socket_link<UDP_mcast_link<ASPECTs ...>, ASPECTs ...>,
                      public impl::filter_impl_for<ASPECTs ...>::type, // EBO
                      public impl::group_socket_handle_for<ASPECTs ...>::type // EBO
{
    private: // ..............................................................

        using super                 = socket_link<UDP_mcast_link<ASPECTs ...>, ASPECTs ...>;
        using filter_impl           = typename impl::filter_impl_for<ASPECTs ...>::type;
        using group_socket_handle   = typename impl::group_socket_handle_for<ASPECTs ...>::type;

    public: // ...............................................................

        UDP_mcast_link (std::string const & ifc_name, std::vector<net::mcast_source> const & sources, recv_arg_map const & recv_args) :
            super (recv_args, impl::open_mcast_recv_link (ifc_name, SOCK_RAW, recv_args.get<net::ts_policy> ("tsp", net::ts_policy::hw_fallback_to_sw)), /* link_name */ifc_name),
            filter_impl (recv_args, & sources.front ()), // HACK
            group_socket_handle (impl::join_mcast_sources (ifc_name, sources))
        {
            vr_static_assert (super::link_mode () == mode::recv); // catch ifc usage errors early at compile-time
        }

        UDP_mcast_link (std::string const & ifc_name, send_arg_map const & send_args) :
            super (send_args, impl::open_mcast_send_link (ifc_name, SOCK_RAW), /* link_name */ifc_name),
            filter_impl ({ }, nullptr)
        {
            vr_static_assert (super::link_mode () == mode::send); // catch ifc usage errors early at compile-time
        }


        ~UDP_mcast_link () VR_NOEXCEPT // calls 'close ()'
        {
            close ();
        }

        void close () VR_NOEXCEPT; // idempotent


        // link_base:

        std::pair<addr_const_t, capacity_t> recv_poll_impl ();

        capacity_t send_flush_impl (capacity_t const len);

    private: // ..............................................................


        // TODO like the capture version, design is limited to reading at most a single packet per invocation;
        // it would be nice to leverage something like recvmmsg() but it's hard with zero-copy buffering and
        // also if ef_vi is in the future

        VR_FORCEINLINE void recv_poll_impl (util::bool_constant<false> /* do timestamping */);
        VR_FORCEINLINE void recv_poll_impl (util::bool_constant<true>  /* do timestamping */);

        using super::m_socket; // this is used as multicast link [group membership is maintained by 'group_socket_handle' in 'recv' mode]

}; // end of class
//............................................................................

template<typename ... ASPECTs>
void
UDP_mcast_link<ASPECTs ...>::close () VR_NOEXCEPT
{
    if (super::has_state () && (super::state () == link_state::closed))
        return;

    group_socket_handle::close ();
    super::close (); // chain

    if (super::has_state ()) super::state () = link_state::closed;
}
//............................................................................

template<typename ... ASPECTs>
void
UDP_mcast_link<ASPECTs ...>::recv_poll_impl (util::bool_constant<false>)
{
    typename super::recv_impl & ifc = super::recv_ifc ();
    assert_positive (ifc.w_window ()); // caller ensures

    capacity_t rc = ::recv (m_socket.fd (), ifc.w_position (), ifc.w_window (), MSG_DONTWAIT);
    if (VR_LIKELY (rc < 0)) // frequent case for a non-blocking socket
    {
        auto const e = errno;
        if (VR_UNLIKELY (e != EAGAIN))
            throw_x (io_exception, "recv() error (" + string_cast (e) + "): " + std::strerror (e));

        rc = 0; // EAGAIN
    }

    if (rc > 0)
    {
        if (! (super::has_recv_filter () && filter_impl::drop (ifc.w_position (), rc)))
        {
            DLOG_trace2 << "  recv_poll rc: " << rc;
            ifc.w_advance (rc);
        }
    }
}

template<typename ... ASPECTs>
void
UDP_mcast_link<ASPECTs ...>::recv_poll_impl (util::bool_constant<true>)
{
    typename super::recv_impl & ifc = super::recv_ifc ();
    assert_positive (ifc.w_window ()); // caller ensures

    super::reset_ancillary (ifc.w_position (), ifc.w_window ());

    capacity_t rc = ::recvmsg (m_socket.fd (), super::mhdr (), MSG_DONTWAIT);
    if (VR_LIKELY (rc < 0)) // frequent case for a non-blocking socket
    {
        auto const e = errno;
        if (VR_UNLIKELY (e != EAGAIN))
            throw_x (io_exception, "recv() error (" + string_cast (e) + "): " + std::strerror (e));

        rc = 0; // EAGAIN
    }

    if (rc > 0)
    {
        if (! (super::has_recv_filter () && filter_impl::drop (ifc.w_position (), rc)))
        {
            // in a searing example of premature optimization, this is hardcoded to assume that
            // timestamping is on and the timestamp(s) is(are) the only ancillary data expected;
            // i.e. there is no loop over all CMSGs -- the first one is expected to be the only
            // one and of type SCM_TIMESTAMPING:

            ::cmsghdr * const cmsg = CMSG_FIRSTHDR (super::mhdr ());
            assert_nonnull (cmsg);
            assert_eq (cmsg->cmsg_type, SCM_TIMESTAMPING);

            ::timespec const & ts = (reinterpret_cast<::timespec const *> (CMSG_DATA (cmsg))) [m_socket.rx_timestamp_policy ()]; // HACK enum values are chosen to skip the unused slot #1

            super::ts_last_recv () = (ts.tv_sec * _1_second () + ts.tv_nsec);

            DLOG_trace2 << "  recv_poll rc: " << rc << ", ts: " << super::ts_last_recv ();
            ifc.w_advance (rc);
        }
    }
}
//............................................................................

template<typename ... ASPECTs>
std::pair<addr_const_t, capacity_t>
UDP_mcast_link<ASPECTs ...>::recv_poll_impl ()
{
    vr_static_assert (super::link_mode () == mode::recv); // catch ifc usage errors early at compile-time

    typename super::recv_impl & ifc = super::recv_ifc ();

    recv_poll_impl (util::bool_constant<super::has_ts_last_recv ()> {}); // throws on an error other than 'EAGAIN'

    return { ifc.r_position (), ifc.size () }; // notes: this returns totals (since last 'recv_flush()')
}
//............................................................................

template<typename ... ASPECTs>
capacity_t
UDP_mcast_link<ASPECTs ...>::send_flush_impl (capacity_t const len)
{
    vr_static_assert (super::link_mode () == mode::send); // catch ifc usage errors early at compile-time

    DLOG_trace2 << "send_flush(" << len << "): entry";
    assert_positive (len); // 'link_base' ensures

    typename super::send_impl & ifc = super::send_ifc ();

    capacity_t const rc = impl::mcast_send (m_socket, ifc.r_position (), len);

    if (rc > 0)
    {
        if (super::has_ts_last_send ()) // track the latest noz-zero write
        {
            super::ts_last_send () = sys::realtime_utc ();
        }

        DLOG_trace3 << "  send_flush rc: " << rc;
        assert_le (rc, ifc.size ()); // design invariant

        ifc.r_advance (rc);
    }

    if (rc > 0) DLOG_trace2 << "send_flush(" << len << "): exit (wrote " << rc << ')'; // TODO not merging in the above block because that's changing to use hw ts
    return rc;
}

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
