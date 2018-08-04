#pragma once

#include "vr/util/type_traits.h"

//----------------------------------------------------------------------------
namespace vr
{
//............................................................................

template<typename ... TAGs>
struct generic_trait_set: public TAGs ...   { };

//............................................................................
//............................................................................

namespace container
{

struct trait        { };

template<bool ENABLED>
struct fixed_capacity: virtual trait    { };

template<bool ENABLED>
struct hash_is_fast: virtual trait      { };

} // end of 'container'
//............................................................................
//............................................................................

namespace io
{
    struct exceptions       { };
    struct no_exceptions    { };

} // end of 'io'
//............................................................................
//............................................................................

namespace thread
{
    struct safe     { };
    struct unsafe   { };

} // end of 'thread'

} // end of namespace
//----------------------------------------------------------------------------
