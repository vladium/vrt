#pragma once

#include "vr/enums.h"
#include "vr/io/links/TCP_link.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
//............................................................................

/**
 * like what @ref market::ASX::execution_link impl uses, except the buffer is
 * a non-persistent type
 */
using mock_ouch_data_link   = TCP_link<recv<_timestamp_, _ring_>, send<_timestamp_, _ring_>>;

constexpr int32_t part_count ()     { return 4; } // ASX-specific TODO match to 'execution_link::poll_descriptor::width()' without adding a ton of include deps

//............................................................................

struct client_connection // movable
{
    using data_link         = mock_ouch_data_link;

    VR_ENUM (state,
        (
            login,
            serving,
            closing,
            closed
        ),
        printable

    ); // end of enum

    client_connection (std::unique_ptr<data_link> && link) :
        m_data_link { std::move (link) }
    {
    }

    VR_ASSUME_COLD void close () VR_NOEXCEPT; // idempotent, resets 'm_data_link'


    std::unique_ptr<data_link> m_data_link;
    int64_t m_send_committed { };
    int64_t m_send_flushed { };
    state::enum_t m_state { state::login };

}; // end of class

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
