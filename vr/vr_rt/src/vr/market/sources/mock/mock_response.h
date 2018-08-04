#pragma once

#include "vr/market/sources/mock/defs.h"
#include "vr/util/timer_event_queue.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
class mock_response; // forward

//............................................................................

using time_action_queue     = util::timer_event_queue<std::unique_ptr<mock_response>>;

//............................................................................

struct mock_response
{
    mock_response (io::client_connection & cc, timestamp_t const ts_start) :
        m_cc { cc },
        m_ts_start { ts_start }
    {
        assert_positive (m_ts_start);
    }

    virtual ~mock_response ()   = default;

    /**
     * @return 'true' if this was the last eval step, 'false' if needs to be called again
     */
    virtual bool evaluate (timestamp_t const now_utc)   = 0;

    io::client_connection & m_cc;
    timestamp_t const m_ts_start; // as scheduled

}; // end of class
//............................................................................

struct ouch_heartbeat final: public mock_response
{
    using mock_response::mock_response;

    // mock_response:

    bool evaluate (timestamp_t const now_utc) override;

}; // end of class

struct ouch_response: public mock_response // movable
{
    ouch_response (io::client_connection & cc, timestamp_t const ts_start,
                   std::vector<int32_t> && len_steps, std::unique_ptr<int8_t []> && response) :
        mock_response (cc, ts_start),
        m_len_steps { std::move (len_steps) }, // last use of 'len_steps'
        m_response { std::move (response) } // last use of 'response'
    {
    }

    // mock_response:

    bool evaluate (timestamp_t const now_utc) override;


    std::vector<int32_t> const m_len_steps;
    std::unique_ptr<int8_t []> m_response { }; // of size that is the sum of 'm_len_steps'
    int32_t m_step { };

}; // end of class

struct ouch_response_state_change: public ouch_response
{
    ouch_response_state_change (io::client_connection & cc, timestamp_t const ts_start,
                                std::vector<int32_t> && len_steps, std::unique_ptr<int8_t []> && response,
                                io::client_connection::state::enum_t const new_state) :
        ouch_response (cc, ts_start, std::move (len_steps), std::move (response)),
        m_new_state { new_state }
    {
    }

    // mock_response:

    bool evaluate (timestamp_t const now_utc) override;

    io::client_connection::state::enum_t const m_new_state;

}; // end of class

} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
