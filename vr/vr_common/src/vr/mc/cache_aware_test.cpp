
#include "vr/mc/cache_aware.h"

#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace mc
{
//............................................................................

struct empty { };
using cl_padded_empty       = cache_line_padded_field<empty>;

vr_static_assert (! (sizeof (cl_padded_empty) % sys::cpu_info::cache::static_line_size ()));
vr_static_assert (std::alignment_of<cl_padded_empty>::value == sys::cpu_info::cache::static_line_size ());

using cl_padded_int8_t      = cache_line_padded_field<int8_t>;

vr_static_assert (! (sizeof (cl_padded_int8_t) % sys::cpu_info::cache::static_line_size ()));
vr_static_assert (std::alignment_of<cl_padded_int8_t>::value == sys::cpu_info::cache::static_line_size ());

struct cl_bytes
{
   int8_t m_bytes [sys::cpu_info::cache::static_line_size ()];

}; // end of class

using cl_padded_cl_bytes    = cache_line_padded_field<cl_bytes>;

vr_static_assert (sizeof (cl_padded_cl_bytes) == sys::cpu_info::cache::static_line_size ());
vr_static_assert (std::alignment_of<cl_padded_cl_bytes>::value == sys::cpu_info::cache::static_line_size ());

struct clp1_bytes
{
   int8_t m_bytes [sys::cpu_info::cache::static_line_size () + 1];

}; // end of class

using cl_padded_clp1_bytes  = cache_line_padded_field<clp1_bytes>;

vr_static_assert (sizeof (cl_padded_clp1_bytes) == 2 * sys::cpu_info::cache::static_line_size ());
vr_static_assert (std::alignment_of<cl_padded_clp1_bytes>::value == sys::cpu_info::cache::static_line_size ());

// non-POD 'T':

using cl_padded_string      = cache_line_padded_field<std::string>;

vr_static_assert (! (sizeof (cl_padded_string) % sys::cpu_info::cache::static_line_size ()));
vr_static_assert (std::alignment_of<cl_padded_string>::value == sys::cpu_info::cache::static_line_size ());

//............................................................................

TEST (cache_line_padded_field, primitive_types)
{
    cl_padded_int8_t const pb1 { 'A' };
    cl_padded_int8_t pb2 { 'A' };

    EXPECT_EQ (static_cast<int8_t const &> (pb1), 'A');
    EXPECT_EQ (static_cast<int8_t const &> (pb2), 'A');

    int8_t & ps2v = pb2;
    ps2v = 'B';

    EXPECT_EQ (static_cast<int8_t const &> (pb2), 'B');

    cl_padded_int8_t pba [10];
    vr_static_assert (sizeof (pba) == 10 * sys::cpu_info::cache::static_line_size ());
}

TEST (cache_line_padded_field, pointer_types)
{
    int8_t dummy { };
    cache_line_padded_field<addr_t> const pv { & dummy };

    vr_static_assert (sizeof (pv) == sys::cpu_info::cache::static_line_size ());
}

TEST (cache_line_padded_field, class_types)
{
    cl_padded_string const ps1 { "aaaa" };
    cl_padded_string ps2 { std::string (4, 'a') }; // explicit temp string to work around std::forward warning in gcc 4.8

    EXPECT_EQ (static_cast<std::string const &> (ps1), "aaaa");
    EXPECT_EQ (static_cast<std::string const &> (ps2), "aaaa");

    std::string & ps2v = ps2;
    ps2v = "xyz";

    EXPECT_EQ (static_cast<std::string const &> (ps2), "xyz");

    cl_padded_string psa [10];
    vr_static_assert (sizeof (psa) == 10 * sys::cpu_info::cache::static_line_size ());
}

} // end of 'mc'
} // end of namespace
//----------------------------------------------------------------------------
