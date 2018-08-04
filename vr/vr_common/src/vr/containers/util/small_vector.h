#pragma once

#include <boost/container/small_vector.hpp>

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{

template<typename T, std::size_t N>
using small_vector          = boost::container::small_vector<T, N>;

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
