
#include "vr/io/events/event_context.h"
#include "vr/io/exceptions.h"
#include "vr/io/files.h"
#include "vr/io/links/link_factory.h"
#include "vr/io/links/TCP_link.h"
#include "vr/io/links/UDP_mcast_link.h"
#include "vr/io/net/io.h" // print() override for ::ip
#include "vr/io/net/socket_factory.h"
#include "vr/io/net/utility.h" // min_size_or_zero, make_group_range_filter
#include "vr/io/pcap/pcap_reader.h"
#include "vr/io/stream_factory.h"
#include "vr/mc/spinflag.h"

#include "vr/test/data.h"
#include "vr/test/mc.h"
#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
//............................................................................
//............................................................................
namespace
{
using namespace io::net;

using stop_flag             = mc::spinflag<>;

//............................................................................

struct IP_mcast_io_base
{
    static constexpr int32_t eth_hdr_len ()     { return sizeof (::ether_header); }
    static constexpr int32_t min_size ()        { return (eth_hdr_len () + sizeof (::ip)); } // raw

}; // end of class
//............................................................................

struct IP_mcast_reader: public IP_mcast_io_base
{
    template<typename CTX>
    int32_t
    consume (CTX & ctx, addr_const_t/* ::ether_header */const data, int32_t const packet_size)
    {
        ::ip const * const ip_hdr = static_cast<::ip const *> (addr_plus (data, eth_hdr_len ()));

        DLOG_trace1 << '[' << m_recv_count << "] received raw packet of size " << packet_size << " byte(s): " << print (* ip_hdr);

        ++ m_recv_count;

        return packet_size;
    }

    int64_t m_recv_count { };

}; // end of class

template<typename SEND_LINK>
struct IP_mcast_sender: public IP_mcast_io_base
{
    IP_mcast_sender (SEND_LINK & link, int64_t const send_limit) :
        m_send_link { link },
        m_send_limit { send_limit }
    {
        check_positive (send_limit);
    }

    /*
     * consume the captured packet at 'data' and send it into 'm_send_link'
     */
    template<typename CTX>
    int32_t
    consume (CTX & ctx, addr_const_t/* ::ether_header */const data, int32_t const packet_size)
    {
        vr_static_assert (has_field<_ts_local_, CTX> ());

        if (m_send_count >= m_send_limit)
            throw_cfx (stop_iteration);

        ::ip const * const ip_hdr = static_cast<::ip const *> (addr_plus (data, eth_hdr_len ()));

        if (ip_hdr->ip_p == IPPROTO_UDP) // use only UDP [mcast] packets
        {
            timestamp_t const ts_local = field<_ts_local_> (ctx);
            timestamp_t const ts_local_prev = m_ts_local_prev;

            if (VR_LIKELY (ts_local_prev))
            {
                int64_t const delay = std::min ((ts_local - ts_local_prev), 2 * _1_microsecond ());
                DLOG_trace2 << "replay delay " << delay << " ns (actual ts_local delta: " << (ts_local - ts_local_prev) << " ns)";
                sys::short_sleep_for (delay);
            }
            m_ts_local_prev = ts_local;


            DLOG_trace2 << '[' << m_send_count << "] sending raw packet of size " << packet_size << " byte(s): " << print (* ip_hdr);

            addr_t const packet = m_send_link.send_allocate (packet_size, /* blank init */false);
            std::memcpy (packet, data, packet_size);

            auto const rc = m_send_link.send_flush (packet_size); // note: narrows from 'capacity_t'
            check_eq (rc, packet_size);

            ++ m_send_count;
        }

        return packet_size;
    }

    SEND_LINK & m_send_link;
    int64_t const m_send_limit;
    int64_t m_send_count { };
    timestamp_t m_ts_local_prev { };

}; // end of class
//............................................................................

template<typename RECV_LINK>
struct mcast_client
{
    using receiver          = IP_mcast_reader;   // no pcap encapsulation added (raw socket packet)
    using consume_ctx       = event_context<_ts_local_, _ts_origin_, net::_packet_index_, net::_dst_port_>;

    static constexpr int32_t min_available  = net::min_size_or_zero<receiver>::value ();


    mcast_client (std::string const & ifc, net::mcast_source const & ms, int32_t const capacity, stop_flag & stop_requested) :
        m_filter { net::make_group_range_filter (ms) },
        m_recv_link { mcast_link_factory<RECV_LINK, typename RECV_LINK::recv_buffer_tag>::create ("mcast_client", ifc, { ms }, capacity, { { "root", test::unique_test_path () } }) },
        m_stop_requested { stop_requested }
    {
    }

    int64_t const & recv_count () const
    {
        return m_receiver.m_recv_count;
    }

    void operator() ()
    {
        consume_ctx ctx { };

        try
        {
            while (! m_stop_requested.is_raised ())
            {
                std::pair<addr_const_t, capacity_t> const rc = m_recv_link->recv_poll (); // non-blocking read

                auto const available = rc.second;

                if (available >= min_available) // a single datagram TODO this check is not needed in case of actual socket traffic
                {
                    ::ip const * const ip_hdr = static_cast<::ip const *> (addr_plus (rc.first, IP_mcast_io_base::eth_hdr_len ()));

                    if (m_filter (ip_hdr->ip_dst.s_addr)) // single-branch test
                    {
                        int32_t const consumed = m_receiver.consume (ctx, rc.first, available);
                        check_eq (consumed, available);
                    }

                    m_recv_link->recv_flush (available);
                }
            }
        }
        catch (stop_iteration const &)
        {
            LOG_trace1 << "client requested an early stop";
        }
        catch (std::exception const & e)
        {
            LOG_error << "I/O failure in client: " << exc_info (e);
        }
        LOG_info << "client DONE (received " << recv_count () << " packet(s))";
    }

    IP_range_filter const m_filter;
    std::unique_ptr<RECV_LINK> m_recv_link; // non-const: movable
    receiver m_receiver { };
    stop_flag & m_stop_requested;

}; // end of class

template<typename SEND_LINK>
struct mcast_server
{
    using in_reader         = pcap_reader<>;                // pcap framing-aware capture reader
    using sender            = IP_mcast_sender<SEND_LINK>;   // pcap encapsulation added to the visitor

    mcast_server (std::istream & in, std::string const & ifc, int32_t const capacity, stop_flag & client_stop_flag, int64_t const send_limit) :
        m_in { in },
        m_send_link { mcast_link_factory<SEND_LINK, typename SEND_LINK::send_buffer_tag>::create ("mcast_server", ifc, capacity, { { "root", test::unique_test_path () } }) },
        m_sender { * m_send_link, send_limit }, // 'sender' is also where send-side throttling happens
        m_client_stop_flag { client_stop_flag }
    {
    }

    int64_t const & send_count () const
    {
        return m_sender.m_send_count;
    }

    void operator() ()
    {
        in_reader r { m_in };

        try
        {
            r.evaluate (m_sender);
        }
        catch (std::exception const & e)
        {
            LOG_error << "I/O failure in server: " << exc_info (e);
        }

        // give all packets a chance to be received:

        sys::long_sleep_for (2 * _1_second ()); // TODO make the test less probabilistic

        LOG_info << "server DONE (sent " << send_count () << " packet(s))";
        m_client_stop_flag.raise ();
    }

    std::istream & m_in;
    std::unique_ptr<SEND_LINK> m_send_link; // non-const: movable
    sender m_sender;
    stop_flag & m_client_stop_flag;

}; // end of class
//............................................................................
//............................................................................

struct simple_message_hdr final
{
    int64_t m_len; // "payload" length NOT inclusive of this header

}; //end of class
//............................................................................

struct tcp_recv
{
    static constexpr int32_t min_size ()        { return sizeof (simple_message_hdr); }

    template<typename CTX>
    int32_t
    consume (CTX & ctx, addr_const_t const data, int32_t const available)
    {
        check_ge (available, min_size ()); // design guarantee

        simple_message_hdr const * const hdr = static_cast<simple_message_hdr const *> (data);
        int32_t const msg_len = min_size () + hdr->m_len;

        if (available < msg_len)
            return -1; // partial read

        DLOG_trace1 << '[' << m_recv_count << "] received simple message, payload len " << hdr->m_len << " byte(s) (available: " << available << ')';

        ++ m_recv_count;
        m_recv_size += msg_len;

        return msg_len;
    }

    int64_t m_recv_count { };
    int64_t m_recv_size { };

}; // end of class


template<typename LINK>
struct tcp_peer
{
    using receiver          = tcp_recv;
    using consume_ctx       = event_context<_ts_local_, _ts_origin_, net::_packet_index_>; // not actually used by the test 'receiver'

    static constexpr int32_t min_available ()   { return net::min_size_or_zero<receiver>::value (); }

    static constexpr int32_t len_limit  = 16 * 1024;

    /*
     * client
     */
    tcp_peer (std::string const & server, std::string const & service, int32_t const capacity, stop_flag & server_stop_flag, int64_t const seed, arg_map const & opt_args = { }) :
        m_server { server },
        m_service { service },
        m_capacity { capacity },
        m_seed { seed },
        m_send_limit { opt_args.get<int64_t> ("send_limit", 0) },
        m_server_stop_flag { server_stop_flag }
    {
    }

    /*
     * server
     */
    tcp_peer (std::string const & service, int32_t const capacity, stop_flag & server_stop_flag, int64_t const seed) :
        tcp_peer { "", service, capacity, server_stop_flag, seed }
    {
    }

    int64_t const & recv_count () const
    {
        return m_receiver.m_recv_count;
    }

    int64_t const & recv_size () const
    {
        return m_receiver.m_recv_size;
    }


    int64_t const & send_count () const
    {
        return m_send_count;
    }

    int64_t const & send_size () const
    {
        return m_send_size;
    }

    void operator() ()
    {
        bool const is_server = m_server.empty ();
        consume_ctx ctx { };

        string_literal_t const name = (is_server ? "server" : "client");
        uint64_t rnd = m_seed; // note: unsigned

        try
        {
            if (is_server)
            {
                net::socket_handle lsh { net::socket_factory::create_TCP_server (m_service) };
                net::socket_handle csh { lsh.accept (m_server_stop_flag, _1_second ()) };

                m_link = tcp_link_factory<LINK>::create ("tcp_server", std::move (csh), m_capacity); // last use of 'csh'
            }
            else
            {
                for (int32_t retry = 0; retry < 9; ++ retry)
                {
                    try
                    {
                        m_link = tcp_link_factory<LINK>::create ("tcp_client", m_server, m_service, m_capacity);
                        break;
                    }
                    catch (io_exception const &)
                    {
                        LOG_warn << '[' << retry << "] retrying connecting to server ...";

                        sys::long_sleep_for ((10 << retry) * _1_millisecond ());
                    }
                }
            }

            // note: unlike the mcast case, TCP client starts and stops the data exchange:

            if (! is_server)
            {
                send_message (1, rnd);
            }

            while (true)
            {
                std::pair<addr_const_t, capacity_t> const rc = m_link->recv_poll (); // non-blocking read

                auto available = rc.second;

                if (available >= min_available ()) // a message header is guaranteed to have been read entirely
                {
                    int32_t consumed { };

                    do // loop over all full messages in the buffer
                    {
                        int32_t const rrc = m_receiver.consume (ctx, addr_plus (rc.first, consumed), available);
                        if (rrc < 0)
                            break; // partial message

                        // send/echo a message:

                        int32_t const parts = std::min<int32_t> (1 + (test::next_random (rnd) % 8), rrc);
                        assert_positive (parts);

                        if (is_server) // server echos unconditionally, client only if haven't received all ACKs
                        {
                            echo_message (rc.first, parts, rnd);
                        }
                        else if (send_count () < m_send_limit)
                        {
                            send_message (parts, rnd);
                        }

                        available -= rrc;
                        consumed += rrc;

                        assert_nonnegative (available);
                        assert_le (consumed, rc.second);
                    }
                    while (available >= min_available ());

                    if (consumed) m_link->recv_flush (consumed);
                }

                // check if we are done with the test:

                if (is_server)
                {
                    if (m_server_stop_flag.is_raised ())
                        break;
                }
                else
                {
                    if (recv_count () >= m_send_limit) // end of test
                    {
                        m_server_stop_flag.raise (); // tell server
                        break;
                    }
                }
            }
        }
        catch (stop_iteration const &)
        {
            LOG_trace1 << name << " requested an early stop";
        }
        catch (std::exception const & e)
        {
            LOG_error << "I/O failure in " << name << ": " << exc_info (e);
        }
        LOG_info << name << " DONE (received " << recv_count () << " packet(s))";
    }

    /*
     * client
     */
    void send_message (int32_t const parts, uint64_t & rnd)
    {
        int32_t const len = min_available () + (test::next_random (rnd) % len_limit);

        assert_ge (len, min_available ());
        assert_ge (len, parts);

        addr_t const msg_buf = m_link->send_allocate (len); // blank-filled by default
        {
            simple_message_hdr * const hdr = static_cast<simple_message_hdr *> (msg_buf);
            hdr->m_len = len - sizeof (simple_message_hdr);
        }
        send_flush (len, parts, rnd);

        ++ m_send_count;
        m_send_size += len;
    }

    /*
     * server
     */
    void echo_message (addr_const_t const data, int32_t const parts, uint64_t & rnd)
    {
        simple_message_hdr const * const hdr = static_cast<simple_message_hdr const *> (data);
        int32_t const len = hdr->m_len + sizeof (simple_message_hdr);

        assert_ge (len, parts);

        addr_t const msg_buf = m_link->send_allocate (len, false);
        {
            std::memcpy (msg_buf, data, len); // header + data
        }
        send_flush (len, parts, rnd);

        ++ m_send_count;
        m_send_size += len;
    }

    /*
     * simulate a randomized/partialized send:
     */
    void send_flush (int32_t const len, int32_t const parts, uint64_t & rnd)
    {
        std::vector<int32_t> const len_parts = test::random_range_split (len, parts, rnd);
        DLOG_trace2 << "len_parts: " << print (len_parts);

        for (int32_t i = 0; i < parts; ++ i)
        {
            int64_t const pause = (test::next_random (rnd) % 20) - 10;
            if (pause <= 0)
                boost::this_thread::yield ();
            else
                sys::short_sleep_for (pause * _1_microsecond ());

            m_link->send_flush (len_parts [i]);
        }
    }

    std::string m_server { };
    std::string m_service { };
    int32_t const m_capacity;
    std::unique_ptr<LINK> m_link;   // non-const: movable
    int64_t const m_seed;
    int64_t const m_send_limit;     // used in client mode only
    int64_t m_send_count { };
    int64_t m_send_size { };
    receiver m_receiver { };
    stop_flag & m_server_stop_flag;

}; // end of class
//............................................................................

using buffer_tags        = gt::Types
<
    _ring_,
    _tape_
>;

template<typename T> struct socket_link_test: public gt::Test { };
TYPED_TEST_CASE (socket_link_test, buffer_tags);

} // end of anonymous
//............................................................................
//............................................................................
/*
 * TODO vary send/recv buffer tags independently
 *
 * confirm that it's actually possible to send/recv [raw] multicast packets within
 * the testsuite
 */
TYPED_TEST (socket_link_test, raw_mcast)
{
    using buffer_tag        = TypeParam; // test parameter

    fs::path const test_input { test::data_dir () / "p1p2.itch.20180612.extract.pcap.zst" };
    constexpr int64_t send_limit    = 1009;

    LOG_info << "using test data (first " << send_limit << " UDP packet(s)) from " << print (test_input);

    std::unique_ptr<std::istream> const in = stream_factory::open_input (test_input);

    using recv_link         = UDP_mcast_link<recv<_timestamp_, buffer_tag>>;
    using send_link         = UDP_mcast_link<send<_timestamp_, buffer_tag>>;

    std::string const ifc { test::mcast_ifc () };
    net::mcast_source const ms { "203.0.119.212->233.71.185.8, 233.71.185.9, 233.71.185.10, 233.71.185.11, 233.71.185.12" };
    int32_t const capacity { 64 * 1024 }; // replicates 'market::itch_data_link_capacity ()'

    LOG_info << "using test multicast setup " << print (ms);

    using client_task       = mcast_client<recv_link>;
    using server_task       = mcast_server<send_link>;

    stop_flag client_stop_flag { }; // cl-padded

    test::task_container tasks { };

    tasks.add ({ client_task { ifc, ms, capacity, client_stop_flag } }, "client");
    tasks.add ({ server_task { * in, ifc, capacity, client_stop_flag, send_limit } }, "server");

    tasks.start ();
    tasks.stop ();

    client_task const & c = tasks ["client"];
    server_task const & s = tasks ["server"];

    LOG_info << "sent " << s.send_count () << " packet(s), received " << c.recv_count ();

    ASSERT_EQ (s.send_count (), send_limit);
    EXPECT_EQ (c.recv_count (), s.send_count ());
}
//............................................................................
// TODO
// - multiple messages per write
// - test TCP server disconnect handling

/*
 * TCP is straightforward but test working with non-blocking sockets, both endpoints;
 * this also tests correctness of framing logic in the presence of random I/O delays
 * and partial TCP socket writes
 */
TYPED_TEST (socket_link_test, tcp_duplex)
{
    using buffer_tag        = TypeParam; // test parameter

    constexpr int64_t send_limit    = VR_IF_THEN_ELSE (VR_DEBUG)(2000, 4000);

    int64_t const seed = test::env::random_seed<int64_t> ();

    using link              = TCP_link<recv<_timestamp_, buffer_tag>, send<_timestamp_, buffer_tag>>;

    // TODO try different 'capacity' values
    // TODO scale transmitted data volume with 'capacity'

    int32_t const capacity { 256 * 1024 }; // replicates 'market::ouch_link_capacity ()'

    using client_task       = tcp_peer<link>;
    using server_task       = tcp_peer<link>;

    stop_flag server_stop_flag { }; // cl-padded

    test::task_container tasks { };

#define vr_SERVER_PORT "16661"

    // client determines the duration of data (echo) exchange

    tasks.add ({ client_task { "localhost", vr_SERVER_PORT, capacity, server_stop_flag, seed,
                                 {
                                     { "send_limit", send_limit },
                                 } } },
        "client");
    tasks.add ({ server_task { vr_SERVER_PORT, capacity, server_stop_flag, seed } },
        "server");

    server_task const & s = tasks ["server"];
    client_task const & c = tasks ["client"];

    tasks.start ();
    tasks.stop ();

    LOG_info << "client sent " << c.send_count () << " packet(s) (" << c.send_size () << " byte(s)), received " << c.recv_count () << " packet(s) (" << c.recv_size () << " byte(s))";
    LOG_info << "server sent " << s.send_count () << " packet(s) (" << s.send_size () << " byte(s)), received " << s.recv_count () << " packet(s) (" << s.recv_size () << " byte(s))";

    // validate message counts:

    ASSERT_EQ (c.send_count (), send_limit);
    EXPECT_EQ (c.recv_count (), send_limit); // client drives starting and stopping the exchange

    EXPECT_EQ (s.recv_count (), c.send_count ()); // server received everything client thinks it's sent
    EXPECT_EQ (s.send_count (), s.recv_count ()); // it's an echo server, after all

    // validate data contents:

    ASSERT_EQ (c.recv_size (), c.send_size ());
    ASSERT_EQ (s.recv_size (), s.send_size ());

    EXPECT_EQ (s.recv_size (), c.send_size ());

#undef  vr_SERVER_PORT
}

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
