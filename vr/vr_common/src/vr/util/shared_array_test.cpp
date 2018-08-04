
#include "vr/util/shared_array.h"

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

struct S: public test::instance_counter<S>
{
    S (int32_t const a0, std::string const & a1) :
        m_f0 { a0 },
        m_f1 { a1 }
    {
    }

    int32_t m_f0;
    std::string m_f1;

}; // end of class
//............................................................................

template<typename VALUE_TYPE, typename POLICY>
struct scenario
{
    using value_type        = VALUE_TYPE;
    using policy            = POLICY;

}; // end of scenario

#define vr_SCENARIO(r, product) ,scenario< BOOST_PP_SEQ_ENUM (product) >

using primitive_type_scenarios      = gt::Types
<
    scenario<int16_t, thread::safe>
    BOOST_PP_SEQ_FOR_EACH_PRODUCT (vr_SCENARIO, ((int8_t)(char)(int32_t)(int64_t)) ((thread::safe)(thread::unsafe)))
>;

using instance_counting_scenarios   = gt::Types
<
    scenario<S, thread::safe>,
    scenario<S, thread::unsafe>
>;

#undef vr_SCENARIO

} // end of anonymous
//............................................................................
//............................................................................

template<typename T> struct shared_array_primitive_types: public gt::Test { };
TYPED_TEST_CASE (shared_array_primitive_types, primitive_type_scenarios);

TYPED_TEST (shared_array_primitive_types, ref_counting)
{
    using scenario      = TypeParam; // test parameter

    using value_type    = typename scenario::value_type;
    using policy        = typename scenario::policy;

    using shared_type   = shared_array<value_type, policy>;

    using ptr_type      = typename shared_type::ptr;

    auto const high_bit = (static_cast<value_type> (1) << (sizeof (value_type) * 8 - 1));


    for (int32_t array_size = 1; array_size < 100000; array_size = (array_size < 64 ? array_size + 1 : array_size * 2 + 1))
    {
        LOG_trace1 << "testing size " << array_size;

        for (int32_t repeat = 0; repeat < 3; ++ repeat)
        {
            ptr_type const p = shared_type::create (array_size);
            ASSERT_TRUE (p);
            ASSERT_EQ (1, p->ref_count ()); // single shared ptr so far
            ASSERT_EQ (array_size, p->size ());

            // test array slots are valid:
            {
                // write high and low bits of 'value_type':

                for (int32_t i = 0; i < array_size; ++ i)
                {
                    p->data ()[i] = (high_bit | i);
                }

                // read them back:

                for (int32_t i = 0; i < array_size; ++ i)
                {
                    value_type const expected = (high_bit | i);

                    ASSERT_EQ (expected, p->data ()[i]) << "failed at index " << i;
                }
            }

            // add ref:

            ptr_type const p2 { p };
            ASSERT_EQ (p.get (), p2.get ());

            EXPECT_EQ (2, p->ref_count ());
        }
    }
}
//............................................................................

template<typename T> struct shared_array_instance_counting_types: public gt::Test { };
TYPED_TEST_CASE (shared_array_instance_counting_types, instance_counting_scenarios);

TYPED_TEST (shared_array_instance_counting_types, ref_counting)
{
    using scenario      = TypeParam; // test parameter

    using value_type    = typename scenario::value_type;
    using policy        = typename scenario::policy;

    using shared_type   = shared_array<value_type, policy>;

    using ptr_type      = typename shared_type::ptr;

    auto const high_bit = (static_cast<int32_t> (1) << (sizeof (int32_t) * 8 - 1));


    auto const ic_start    = S::instance_count ();
    {
        for (int32_t array_size = 1; array_size < 1000; array_size = (array_size < 64 ? array_size + 1 : array_size * 2 + 1))
        {
            LOG_trace1 << "testing size " << array_size;

            for (int32_t repeat = 0; repeat < 3; ++ repeat)
            {
                ptr_type const p = shared_type::create (array_size, 333 + repeat, std::to_string (333 + repeat));
                ASSERT_TRUE (p);
                ASSERT_EQ (1, p->ref_count ()); // single shared ptr so far
                ASSERT_EQ (array_size, p->size ());

                ASSERT_EQ (ic_start + array_size, S::instance_count ());

                // test array slots are valid objects:
                {
                    // constructed state:

                    for (int32_t i = 0; i < array_size; ++ i)
                    {
                        value_type const & obj = p->data ()[i];

                        EXPECT_EQ (333 + repeat, obj.m_f0);
                        EXPECT_EQ (std::to_string (333 + repeat), obj.m_f1);
                    }

                    // mutate slots and read them back (two separate passes):

                    for (int32_t i = 0; i < array_size; ++ i)
                    {
                        value_type & obj = p->data ()[i];

                        obj.m_f0 = (high_bit | i);
                        obj.m_f1 = std::to_string (obj.m_f0);
                    }

                    for (int32_t i = 0; i < array_size; ++ i)
                    {
                        value_type const & obj = p->data ()[i];

                        int32_t const f0_expected = (high_bit | i);

                        ASSERT_EQ (f0_expected, obj.m_f0) << "failed at index " << i;
                        ASSERT_EQ (std::to_string (f0_expected), obj.m_f1) << "failed at index " << i;
                    }
                }

                // add ref:

                ptr_type const p2 { p };
                ASSERT_EQ (p.get (), p2.get ());

                EXPECT_EQ (2, p->ref_count ());

                ASSERT_EQ (ic_start + array_size, S::instance_count ());
            }
        }
    }
    EXPECT_EQ (ic_start, S::instance_count ()); // no 'S' instances leaked
}

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
