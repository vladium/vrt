
#include "vr/market/sources/mock/mock_response.h"

#include "vr/market/net/SoupTCP_.h"

#include <numeric> // std::accumulate

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
//............................................................................

bool
ouch_heartbeat::evaluate (timestamp_t const now_utc)
{
    constexpr int32_t len   = sizeof (SoupTCP_packet_hdr);

    addr_t const msg_buf = m_cc.m_data_link->send_allocate (len, /* special case: no actual low-level protocol payload */false);

    SoupTCP_packet_hdr & hdr = * static_cast<SoupTCP_packet_hdr *> (msg_buf);
    {
        hdr.length () = 1;
        hdr.type () = 'H'; // [SoupTCP] server heartbeat
    }
    m_cc.m_send_committed += len; // note: this just increases commit count, the actual send I/O is coalesced and done elsewhere

    return true;
}
//............................................................................

bool
ouch_response::evaluate (timestamp_t const now_utc)
{
    if (now_utc < m_ts_start)
        return false; // not yet

    if (VR_LIKELY (m_cc.m_state >= io::client_connection::state::closing))
        return true; // no-op

    int32_t const step_limit = m_len_steps.size ();
    int32_t const step = ++ m_step; // side effect

    assert_le (step, step_limit);

    if (step == 1)
    {
        int32_t const len = std::accumulate (m_len_steps.begin (), m_len_steps.end (), 0);
        assert_positive (len);

        addr_t const msg_buf = m_cc.m_data_link->send_allocate (len);
        std::memcpy (msg_buf, m_response.get (), len);
    }

    // sim fractional writes:

    m_cc.m_send_committed += m_len_steps [step -/* ! */1]; // note: this just increases commit count, the actual send I/O is coalesced and done elsewhere

    return (step == step_limit);
}
//............................................................................

bool
ouch_response_state_change::evaluate (timestamp_t const now_utc)
{
    bool const done = ouch_response::evaluate (now_utc); // [chain]

    if (done) m_cc.m_state = m_new_state;

    return done;
}

} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
