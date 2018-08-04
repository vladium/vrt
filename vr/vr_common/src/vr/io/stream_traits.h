#pragma once

#include "vr/io/defs.h"
#include "vr/util/fixed_precision.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
//............................................................................

template<typename ... PARAMs>
struct stream_traits; // master

//............................................................................
// specializations:

template<>
struct stream_traits<enum_<format, format::CSV> >
{
    using price_type                = double;

    static constexpr int32_t digits ()          { return 4; } // 2 ok for US equities, 3 ok for ASX
    static constexpr int32_t digits_secs ()     { return 6; } // us default

    // TODO separator, etc

}; // end of specialization

template<>
struct stream_traits<enum_<format, format::HDF> >
{
    using price_type                = util::scaled_int_t;

    static constexpr int32_t digits ()          { return -1; } // not referenced by this format
    static constexpr int32_t digits_secs ()     { return -1; } // not referenced by this format

    // TODO try to keep column ordering or not, etc

}; // end of specialization


} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
