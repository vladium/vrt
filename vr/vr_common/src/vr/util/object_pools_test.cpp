
#include "vr/sys/os.h"
#include "vr/util/object_pools.h"
#include "vr/util/object_pool_checker.h"

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

// test PODs that all have an 'm_i4' int field (used by the last testcase below):

struct T // tight for default pointer size
{
    int32_t m_i4;

}; // end of class

struct S // size != alignment
{
    std::array<int8_t, 8> m_f8;
    int32_t m_i4;

}; // end of class

struct BIG // object that's larger than OS page (not the target use case, but things should still work)
{
    int8_t _ [sys::os_info::static_page_size ()];
    int32_t m_i4;
    int8_t __;

}; // end of class

} // end of anonymous
//............................................................................
//............................................................................

template
<
    typename SLOT_TYPE, // only used to set size/alignment
    int64_t ALLOC_COUNT,
    int64_t FREE_COUNT
>
struct gs_scenario
{
    using slot_type                         = SLOT_TYPE;
    static constexpr int64_t alloc_count () { return ALLOC_COUNT; }
    static constexpr int64_t free_count ()  { return FREE_COUNT; }

}; // end of scenario

using gs_scenarios  = gt::Types
<
    gs_scenario<T, 109,   10>,
    gs_scenario<T, 109,   109>,  // allow full release
    gs_scenario<T, 1024,  1024>,
    gs_scenario<T, 10009, 9001>,
    gs_scenario<T, 10009, 10009> // allow full release
    ,
    gs_scenario<S, 109,   10>,
    gs_scenario<S, 109,   109>,  // allow full release
    gs_scenario<S, 128,   128>,
    gs_scenario<S, 1009,  901>,
    gs_scenario<S, 1009,  1009>  // allow full release
    ,
    gs_scenario<BIG, 109,   10>,
    gs_scenario<BIG, 109,   109> // allow full release
>;

template<typename T> struct fixed_size_allocator_test: public gt::Test { };
TYPED_TEST_CASE (fixed_size_allocator_test, gs_scenarios);

//............................................................................
/*
 * allocate/release in simple linear patterns, test capacity expansion
 */
TYPED_TEST (fixed_size_allocator_test, grow_and_shrink)
{
    using scenario      = TypeParam; // test parameter

    constexpr std::size_t size          = sizeof (typename scenario::slot_type);
    constexpr std::size_t alignment     = alignof (typename scenario::slot_type);
    constexpr int64_t alloc_count       = scenario::alloc_count ();
    constexpr int64_t free_count        = scenario::free_count ();

    const int32_t rounds        = 20;

    using pool_type         = fixed_size_allocator<size, alignment>;
    using alloc_result      = typename pool_type::alloc_result;


    pool_type pool { };

    pool.check (); // valid state after construction

    std::vector<alloc_result> refs { }; // ref holder

    for (int32_t round = 0; round < rounds; ++ round)
    {
        for (int64_t i = 0; i < alloc_count; ++ i)
        {
            auto const ar = pool.allocate ();

            check_nonnull (ar.first);
            check_zero (integral_cast (ar.first) % alignment); // alignment is at least as requested

            refs.push_back (ar);
        }
        pool.check ();

        for (int64_t i = /* ! */1; i <= free_count; ++ i)
        {
            pool.release (refs [refs.size () - i].second);
        }
        pool.check ();
    }

    pool.check ();
}
//............................................................................
/*
 * allocate/release in randomized manner, test that
 */
TYPED_TEST (fixed_size_allocator_test, random_alloc_pattern)
{
    using scenario      = TypeParam; // test parameter

    using value_type                    = typename scenario::slot_type;

    constexpr std::size_t size          = sizeof (value_type);
    constexpr std::size_t alignment     = alignof (value_type);
    constexpr int64_t alloc_count       = scenario::alloc_count ();
    constexpr int64_t free_count        = scenario::free_count ();

    const int32_t rounds        = 20;

    using pool_type         = fixed_size_allocator<size, alignment>;
    using alloc_result      = typename pool_type::alloc_result;

    using live_map          = std::map<int32_t, alloc_result>; // ordered container by design

    int32_t rnd = test::env::random_seed<int32_t> ();

    pool_type pool { };

    pool.check (); // valid state after construction

    live_map refs { }; // refs that are currently allocated

    for (int32_t round = 0; round < rounds; ++ round)
    {
        LOG_trace1 << "[round " << round << "]: " << refs.size () << " live object(s)";

        // some random number of allocs:

        int64_t const ac = unsigned_cast (rnd) % alloc_count;

        for (int64_t i = 0; i < ac; ++ i)
        {
            test::next_random (rnd);

            auto const ar = pool.allocate ();

            check_nonnull (ar.first);
            check_zero (integral_cast (ar.first) % alignment); // alignment is at least as requested

            value_type & obj = * static_cast<value_type *> (ar.first);
            obj.m_i4 = rnd; // put some data into 'obj'

            refs.emplace (rnd, ar); // key it under the same value
        }
        pool.check ();

        if (! refs.empty ())
        {
            // some random number of frees (but not more than live):

            int64_t const fc = unsigned_cast (rnd) % std::min<int64_t> (free_count, refs.size ());

            for (int64_t i = 0; i < fc; ++ i)
            {
                test::next_random (rnd);

                // pick a somewhat random victim to release:

                auto vi = refs.lower_bound (rnd);
                if (vi == refs.end ()) vi = refs.begin ();
                assert_condition (vi != refs.end ());

                pool.release (vi->second.second);
                refs.erase (vi);
            }
        }
        pool.check ();

        // validate that objects that are live (in 'refs') have data that is matching
        // what we recorded for them:

        for (auto const & kv : refs)
        {
            int32_t const key = kv.first;

            value_type const & obj = * static_cast<value_type const *> (pool [kv.second.second]);

            ASSERT_EQ (obj.m_i4, key);
        }
    }

    pool.check ();
}
//............................................................................
//............................................................................
namespace
{

struct IC final: public virtual test::instance_counter<IC>
{
    IC (int32_t const a0, std::string const & a1) :
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

template
<
    typename T,
    bool RELEASE,
    int64_t ALLOC_COUNT,
    int64_t FREE_COUNT
>
struct obj_scenario
{
    using value_type                        = T;
    static constexpr bool release ()        { return RELEASE; }
    static constexpr int64_t alloc_count () { return ALLOC_COUNT; }
    static constexpr int64_t free_count ()  { return FREE_COUNT; }

}; // end of scenario

using obj_scenarios  = gt::Types
<
    obj_scenario<IC, true, 109,   10>,
    obj_scenario<IC, true, 109,   109>, // allow full release
    obj_scenario<IC, true, 128,   128>,
    obj_scenario<IC, true, 1009,  800>,
    obj_scenario<IC, true, 1009,  1009> // allow full release
    ,
    obj_scenario<IC, false, 109,   10>,
    obj_scenario<IC, false, 109,   109>,
    obj_scenario<IC, false, 128,   128>,
    obj_scenario<IC, false, 1009,  800>,
    obj_scenario<IC, false, 1009,  1009>
>;

template<typename T> struct object_pool_test: public gt::Test { };
TYPED_TEST_CASE (object_pool_test, obj_scenarios);

//............................................................................
/*
 * similar to 'fixed_size_allocator_test.random_alloc_pattern' but using
 * 'object_pool' and verifying that all objects get properly cleaned up (destructed)
 */
TYPED_TEST (object_pool_test, object_lifecycle)
{
    using scenario      = TypeParam; // test parameter

    using value_type                    = typename scenario::value_type;

    constexpr bool    release           = scenario::release (); // if 'false', simulates leaking from the pool
    constexpr int64_t alloc_count       = scenario::alloc_count ();
    constexpr int64_t free_count        = scenario::free_count ();

    const int32_t rounds        = 20;

    using pool_type         = object_pool<value_type>;
    using alloc_result      = typename pool_type::alloc_result;

    using live_map          = std::map<int32_t, alloc_result>; // ordered container by design

    int32_t rnd = test::env::random_seed<int32_t> ();

    live_map refs { }; // refs that are currently allocated

    auto const iIC_start    = IC::instance_count ();
    {
        pool_type pool { };


        for (int32_t round = 0; round < rounds; ++ round)
        {
            LOG_trace1 << "[round " << round << "]: " << refs.size () << " live object(s)";

            // some random number of allocs:

            int64_t const ac = unsigned_cast (rnd) % alloc_count;

            for (int64_t i = 0; i < ac; ++ i)
            {
                test::next_random (rnd);

                auto const ar = pool.allocate (rnd, "string." + string_cast (rnd));

                refs.emplace (rnd, ar); // key it under the same value
            }

            if (! refs.empty ())
            {
                // some random number of frees (but not more than live):

                int64_t const fc = unsigned_cast (rnd) % std::min<int64_t> (free_count, refs.size ());

                for (int64_t i = 0; i < fc; ++ i)
                {
                    test::next_random (rnd);

                    // pick a somewhat random victim to release:

                    auto vi = refs.lower_bound (rnd);
                    if (vi == refs.end ()) vi = refs.begin ();
                    assert_condition (vi != refs.end ());

                    if (release) pool.release (std::get<1> (vi->second));
                    refs.erase (vi);
                }
            }

            // validate that objects that are live (in 'refs') have data that is matching
            // what we recorded for them:

            for (auto const & kv : refs)
            {
                int32_t const key = kv.first;

                value_type & obj = pool [std::get<1> (kv.second)];

                ASSERT_EQ (obj.m_f0, key);
                std::string const f1 = "string." + string_cast (key);
                ASSERT_EQ (obj.m_f1, f1);
            }
        }
    }

    // 'pool' destructed, validate that instance counts return to their starting values:

    EXPECT_EQ (IC::instance_count (), iIC_start);
}

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
