
#include "vr/util/str_const.h"

#include "vr/fw_string.h"

#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
//............................................................................

// check that 'to_fw_string8()' works in constexprs and produces correct layout:

vr_static_assert (str_const ("").to_fw_string8 ()           == 0);

vr_static_assert (str_const ("a").to_fw_string8 ()          == 0x61);
vr_static_assert (str_const ("abc").to_fw_string8 ()        == 0x636261);
vr_static_assert (str_const ("ABCDEFGH").to_fw_string8 ()   == 0x4847464544434241);

//............................................................................
/*
 * check content match wrt 'fw_string8'
 */
TEST (str_const, to_fw_string8)
{
    constexpr str_const cstr { "xyz" };
    uint64_t const cstr_image = cstr.to_fw_string8 ();

    fw_string8 const fws1 { reinterpret_cast<string_literal_t> (& cstr_image), cstr.size () };
    fw_string8 const fws2 { "xyz", 3 };

    EXPECT_EQ (fws1, fws2);
}

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
