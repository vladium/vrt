
#include "vr/util/shared_object.h"

#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
//............................................................................
//............................................................................
namespace
{

template<typename MT_POLICY>
struct S final: public shared_object<S<MT_POLICY> >, public virtual test::instance_counter<S<MT_POLICY> >
{
    S (int32_t const a0, std::string const & a1) :
        m_f0 { a0 },
        m_f1 { a1 }
    {
    }

    int32_t const m_f0;
    std::string const m_f1;

}; // end of class

} // end of anonymous
//............................................................................
//............................................................................

using tested_policies   = gt::Types<thread::safe, thread::unsafe>;

template<typename T> struct shared_object_test: public gt::Test { };
TYPED_TEST_CASE (shared_object_test, tested_policies);

TYPED_TEST (shared_object_test, ref_counting)
{
    using policy        = TypeParam; // test parameter

    using value_type    = S<policy>;
    using shared_type   = value_type; // TODO clean up shared_object<value_type, policy>;

    using ptr_type      = typename shared_type::ptr;

    auto const ic_start    = value_type::instance_count ();
    {
        for (int32_t repeat = 0; repeat < 3; ++ repeat)
        {
            ptr_type const p = shared_type::create (333 + repeat, std::to_string (333 + repeat));
            ASSERT_TRUE (p);
            ASSERT_EQ (1, p->ref_count ()); // single shared ptr so far

            EXPECT_EQ (ic_start + 1, value_type::instance_count ());

            // test object content is valid:
            {
                EXPECT_EQ (333 + repeat, p->m_f0);
                EXPECT_EQ (std::to_string (333 + repeat), p->m_f1);
            }

            // add ref:

            ptr_type const p2 { p };
            ASSERT_EQ (p.get (), p2.get ());

            EXPECT_EQ (2, p->ref_count ());
        }
    }
    EXPECT_EQ (ic_start, value_type::instance_count ()); // no 'S' instances leaked
}

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
