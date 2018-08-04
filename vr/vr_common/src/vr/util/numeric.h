#pragma once

#include "vr/exceptions.h"
#include "vr/strings.h"
#include "vr/util/classes.h"

#include <boost/numeric/conversion/cast.hpp>

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{

template<typename R, typename T>
R
numeric_cast (T const & v)
{
    try
    {
        return boost::numeric_cast<R> (v);
    }
    catch (boost::bad_numeric_cast const & bnc)
    {
        chain_x (type_mismatch, "failed to cast value of type {" + util::class_name<T> () + "} (" + print (v) + ") to {" + util::class_name<R> () + "}:");
    }
}

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
