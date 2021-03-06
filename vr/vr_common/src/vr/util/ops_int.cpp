
#include "vr/util/ops_int.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
//............................................................................
//............................................................................
namespace impl
{

uint64_t const g_ilog10_i64_table_0 [] =
{
     static_cast<uint64_t> (-1),
     9,
     99,
     999,
     9999,
     99999,
     999999,
     9999999,
     99999999,
     999999999,

     9999999999UL,
     99999999999UL,
     999999999999UL,
     9999999999999UL,
     99999999999999UL,
     999999999999999UL,
     9999999999999999UL,
     99999999999999999UL,
     999999999999999999UL,
     9999999999999999999UL
};

uint64_t const g_ilog10_i64_table_1 [] =
{
     0,
     9,
     99,
     999,
     9999,
     99999,
     999999,
     9999999,
     99999999,
     999999999,

     9999999999UL,
     99999999999UL,
     999999999999UL,
     9999999999999UL,
     99999999999999UL,
     999999999999999UL,
     9999999999999999UL,
     99999999999999999UL,
     999999999999999999UL,
     9999999999999999999UL,
};


uint32_t const g_ilog10_i32_table_0 [] =
{
    static_cast<uint32_t> (-1),
    9,
    99,
    999,
    9999,
    99999,
    999999,
    9999999,
    99999999,
    999999999,
    0xFFFFFFFF
};

uint32_t const g_ilog10_i32_table_1 [] =
{
    0,
    9,
    99,
    999,
    9999,
    99999,
    999999,
    9999999,
    99999999,
    999999999,
    0xFFFFFFFF
};

} // end of 'impl'
//............................................................................
//............................................................................
} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
