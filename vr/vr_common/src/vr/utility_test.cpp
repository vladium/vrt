
#include "vr/utility.h"

#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
//............................................................................
//............................................................................
namespace
{

void
throw_something ()
{
    throw 333;
}

} // end of anonymous
//............................................................................
//............................................................................

TEST (scope_guard, auto_var)
{
    {
        std::vector<int32_t> v;
        {
            v.push_back (0);
            auto _ = make_scope_exit ([& v]() { v.clear (); });
            v.push_back (1);
        }
        ASSERT_TRUE (v.empty ());
    }
    {
        std::vector<int32_t> v;
        try
        {
            v.push_back (0);
            auto _ = make_scope_exit ([& v]() { v.clear (); });
            v.push_back (1);
            throw_something ();
        }
        catch (...) { }
        ASSERT_TRUE (v.empty ());
    }
}

TEST (scope_guard, macro)
{
    {
        std::vector<int32_t> v;
        {
            v.push_back (0);
            VR_SCOPE_EXIT ([& v]() { v.clear (); });
            v.push_back (1);
        }
        ASSERT_TRUE (v.empty ());
    }
    {
        std::vector<int32_t> v;
        try
        {
            v.push_back (0);
            VR_SCOPE_EXIT ([& v]() { v.clear (); });
            v.push_back (1);
            throw_something ();
        }
        catch (...) { }
        ASSERT_TRUE (v.empty ());
    }
}


} // end of namespace
//----------------------------------------------------------------------------
