#pragma once

#include "vr/io/links/link_base.h"
#include "vr/io/streams.h"
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

class stream_link_base // TODO is there enough value in having this base?
{
    public: // ...............................................................

        stream_link_base (std::istream & is);
        stream_link_base (std::ostream & os);

    protected: // ............................................................

        addr_t const m_s; // [non-owning] castable to std::istream|std::ostream

}; // end of class

} // end of 'impl'
//............................................................................
//............................................................................
/**
 * a 'stream_link' is mainly useful:
 *
 *  1. for playback/simulation, and
 *  2. as a proof-of-concept that's easy to test
 *
 * @see link_base
 */
template<typename ... ASPECTs>
class stream_link: public impl::stream_link_base, public link_base<stream_link<ASPECTs ...>, ASPECTs ...>
{
    private: // ..............................................................

        using super         = link_base<stream_link<ASPECTs ...>, ASPECTs ...>;

    public: // ...............................................................

        stream_link (std::istream & is, recv_arg_map const & recv_args) : // TODO add vararg overload
            impl::stream_link_base (is),
            super (recv_args)
        {
        }

        stream_link (std::ostream & os, send_arg_map const & send_args) : // TODO add vararg overload
            impl::stream_link_base (os),
            super (send_args)
        {
        }


        // link_base:

        VR_ASSUME_HOT std::pair<addr_const_t, capacity_t> recv_poll_impl ();
        VR_ASSUME_HOT capacity_t send_flush_impl (capacity_t const len);

}; // end of class
//............................................................................

template<typename ... ASPECTs>
std::pair<addr_const_t, capacity_t>
stream_link<ASPECTs ...>::recv_poll_impl ()
{
    // TODO deal with 'is' exception mode

    typename super::recv_impl & ifc = super::recv_ifc ();

    std::istream & is = * static_cast<std::istream *> (m_s);

    capacity_t rc { };
    try
    {
        rc = io::read_fully (is, ifc.w_window (), ifc.w_position ());

        if (rc > 0)
        {
            if (super::has_ts_last_recv ()) // track the latest non-zero read
            {
                super::ts_last_recv () = sys::realtime_utc ();
            }

            DLOG_trace2 << "  recv_poll rc: " << rc;
            ifc.w_advance (rc);
        }
        else
        {
            if (rc == 0) // EOF TODO use iostream API for this
            {
                if (super::has_state ())
                {
                    super::state () = link_state::closed;
                }
            }
//            else
//            {
//                throw_x (io_exception, "istream error"); // TODO
//            }
        }
    }
    catch (...)
    {
        chain_x (io_exception, "read_fully() failed");
    }

    if (rc > 0) DLOG_trace1 << "recv_poll(): exit [read " << rc << ']';
    return { ifc.r_position (), ifc.size () }; // notes: this returns totals (since last 'recv_flush()')
}
//............................................................................

template<typename ... ASPECTs>
capacity_t
stream_link<ASPECTs ...>::send_flush_impl (capacity_t const len)
{
    DLOG_trace1 << "send_flush(" << len << "): entry";
    assert_positive (len); // 'link_base' ensures

    // TODO deal with 'os' exception mode

    typename super::send_impl & ifc = super::send_ifc ();

    std::ostream & os = * static_cast<std::ostream *> (m_s);

    capacity_t rc { };
    try
    {
        rc = io::write_fully (ifc.r_position (), len, os);

        if (rc > 0)
        {
            if (super::has_ts_last_send ()) // track the latest non-zero write
            {
                super::ts_last_send () = sys::realtime_utc ();
            }

            DLOG_trace2 << "  send_flush rc: " << rc;
            assert_le (rc, ifc.size ()); // design invariant

            ifc.r_advance (rc);
        }
//        else
//        {
//            throw_x (io_exception, "ostream error"); // TODO
//        }
    }
    catch (...)
    {
        chain_x (io_exception, "write_fully() failed for len " + string_cast (len));
    }

    DLOG_trace1 << "send_flush(" << len << "): exit [wrote " << rc << ']';
    return rc;
}

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
