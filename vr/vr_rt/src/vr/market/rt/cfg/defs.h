#pragma once

#include "vr/market/defs.h" // liid_t

#include <boost/bimap.hpp>
#include <boost/bimap/set_of.hpp>

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{

struct liid_descriptor
{
    // TODO ? int64_t m_native_iid;
    std::string m_symbol { };

}; // end of class


using symbol_liid_relation  = boost::bimap
                            <
                                boost::bimaps::set_of<std::string>, // symbol (all of those this agent is configured with)
                                boost::bimaps::set_of<liid_t>       // liid (note that these don't necessarily form a contiguous range)
                            >;


struct agent_descriptor
{
    symbol_liid_relation m_symbol_mapping { };

}; // end of class

} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
