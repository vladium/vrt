#pragma once

#include "vr/types.h"

#include <boost/lexical_cast.hpp>

#include <cstdlib> // std::getenv()

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{

template<typename T = std::string>
T
getenv (string_literal_t const name, T const & dflt = { })
{
    char const * v = std::getenv (name); // note: threadsafe starting c++11
    if (v && v [0])
    {
        try
        {
            return boost::lexical_cast<T> (v);
        }
        catch (boost::bad_lexical_cast & blc)
        {
            return dflt;
        }
    }

    return dflt;
}

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
