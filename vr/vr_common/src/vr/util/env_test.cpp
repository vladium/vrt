
#include "vr/util/env.h"

#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{

TEST (getenv, nonexistent)
{
    EXPECT_EQ (1234, getenv<int32_t> ("_nonexistent_var", 1234));
}

TEST (getenv, existent)
{
    ::putenv (const_cast<char *> ("_some_var=1234"));

    EXPECT_EQ ("1234", getenv ("_some_var"));
    EXPECT_EQ (1234, getenv<int32_t> ("_some_var"));
}

TEST (getenv, empty) // empty vars are now treated as unset
{
    ::putenv (const_cast<char *> ("_some_var="));

    EXPECT_EQ ("value_if_empty", getenv<std::string> ("_some_var", "value_if_empty"));
}

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
