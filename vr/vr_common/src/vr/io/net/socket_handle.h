#pragma once

#include "vr/io/net/defs.h"
#include "vr/mc/spinflag.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
namespace net
{
class socket_factory; // forward

class socket_handle final: noncopyable // move-constructible
{
    public: // ...............................................................

        using stop_flag     = mc::spinflag<>;


        socket_handle (socket_handle && rhs);
        ~socket_handle () VR_NOEXCEPT; // calls 'close()'

        void close () VR_NOEXCEPT; // idempotent

        // ACCESSORs:

        int32_t const & fd () const
        {
            assert_nonnegative (m_fd);
            return m_fd;
        }

        int32_t const & family () const
        {
            assert_nonnegative (m_fd);
            return m_family;
        }

        int32_t type () const;

        bool blocking () const;

        int32_t read_buf_size () const;
        int32_t write_buf_size () const;

        ts_policy::enum_t const & rx_timestamp_policy () const
        {
            return m_ts_policy;
        }

        /*
         * 'true' means the socket has been connect()ed (for UDP this does *not* mean there's an actual connection)
         *  or comes from a TCP accept()
         */
        bool const & peer_set () const
        {
            assert_nonnegative (m_fd);
            return m_peer_set;
        }

        addr_and_port peer_name () const;

        // MUTATORs:

        /**
         * @note there is an inherent race condition in draining the RX queue and returning from this method,
         *       i.e. it is possible that some recent datagrams will have arrived when this method completes
         *
         * @return count of datagrams discarded
         */
        int64_t drain_rx_queue ();

        // TODO this sucks, needs to be split into several different sub-APIs:

        // UDP listener:

        /**
         * @param ifc_index [zero means let the kernel assign]
         */
        void join_multicast (sa_descriptor const & group_sa, sa_descriptor const & source_sa, int32_t const ifc_index = { });
        void join_multicast (sa_descriptor const & group_sa, sa_descriptor const & source_sa, std::string const & ifc = { });

        // AF_PACKET IP listener:

        std::pair<uint32_t, uint32_t> statistics_snapshot (); // returns total/dropped packet counts and resets them

        // UDP sender:

        /**
         * @param ifc_index [zero means remove previous assignment]
         */
        void set_multicast_ifc (int32_t const ifc_index = { });
        void set_multicast_ifc (std::string const & ifc = { });

        // TCP server:

        socket_handle accept (stop_flag const & sf, timestamp_t const poll_interval);

        // timestamping:

        ts_policy::enum_t enable_rx_timestamps (int32_t const ifc_index, ts_policy::enum_t const policy = ts_policy::hw_fallback_to_sw);
        ts_policy::enum_t enable_rx_timestamps (std::string const & ifc, ts_policy::enum_t const policy = ts_policy::hw_fallback_to_sw);

    private: // ..............................................................

        friend class socket_factory;

        socket_handle (int32_t const fd, int32_t const family, bool const peer_set);

        int32_t m_fd { -1 };
        int32_t m_family { };
        bitset64_t m_opts_set { };
        ts_policy::enum_t m_ts_policy { ts_policy::hw_fallback_to_sw };
        bool const m_peer_set { false };

}; // end of class

} // end of 'net'
} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
