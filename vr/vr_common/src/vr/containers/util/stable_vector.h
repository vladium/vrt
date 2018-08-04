#pragma once

#include <boost/container/stable_vector.hpp>

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
// TODO for now, boost impl is sufficient but eventually switch to one that doesn't have extra pointer
//      indirection and allows custom size_type (need grow-only version)
/**
 *
 */
template<typename T>
using stable_vector         = boost::container::stable_vector<T>;

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
