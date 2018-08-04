#pragma once

#include "vr/io/defs.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{

struct link_context final
{
    io::pos_t m_pos { };
    VR_IF_DEBUG (timestamp_t m_ts_local_last { -1 };)

}; // end of nested class

struct session_context final
{
    int64_t m_snapshot_seqnum { };  // set on on successful Soup login
    std::string m_session_name { }; // filled in on successful Soup login

}; // end of nested class

} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
