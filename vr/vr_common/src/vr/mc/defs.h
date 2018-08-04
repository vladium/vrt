#pragma once

#include "vr/enums.h"
#include "vr/tags.h"
#include "vr/containers/util/optional_map_adaptor.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace mc
{
//............................................................................

using affinity_map      = util::optional_map_adaptor<int32_t>;

//............................................................................

VR_EXPLICIT_ENUM (rcu_,
    enum_int_t,
        (0b00,          none)
        (0b01,          reader)
        (0b10,          writer)
    ,
    printable

); // end of enum
//............................................................................

struct rcu_marker // @suppress("Class has a virtual method and non-virtual destructor")
{
    virtual rcu_::enum_t rcu_mode () const  = 0;

}; // end of class
//............................................................................

template<typename /* _reader_|_writer_ */MODE_TAG = _reader_>
struct rcu: public rcu_marker // @suppress("Class has a virtual method and non-virtual destructor")
{
    rcu_::enum_t rcu_mode () const final override   { return rcu_::reader; }

}; // end of specialization

template<>
struct rcu<_writer_>: public rcu_marker // @suppress("Class has a virtual method and non-virtual destructor")
{
    rcu_::enum_t rcu_mode () const final override   { return rcu_::writer; }

}; // end of specialization

} // end of 'mc'
} // end of namespace
//----------------------------------------------------------------------------
