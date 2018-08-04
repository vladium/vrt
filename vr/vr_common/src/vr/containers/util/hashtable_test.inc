
//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
//............................................................................
//............................................................................

// jump through some hoops to work around gt::Types<> max limit of 50:

#define vr_HASHTABLE_TEST   BOOST_PP_CAT (hashtable_test_part_, BOOST_PP_ITERATION ())
#define vr_SCENARIOS        BOOST_PP_CAT (scenarios, BOOST_PP_ITERATION ())

//............................................................................

#define vr_HASHTABLES   (default_chained_scatter_table)(fixed_chained_scatter_table)

//#undef vr_KV_TYPES
//#if BOOST_PP_ITERATION () == 1
//#   define vr_KV_TYPES     (int32_t)(int64_t)
//#else
//#   define vr_KV_TYPES     (uint32_t)
//#endif

// TODO temporarily RH table requires K and V to be of unequal sizes

#undef vr_K_TYPES
#define vr_K_TYPES     (int64_t)
#undef vr_V_TYPES
#define vr_V_TYPES     (int32_t)

#undef vr_HASHES
#if BOOST_PP_ITERATION () == 1
#   define vr_HASHES       (identity_hash)(crc32_hash)
#else
#   define vr_HASHES       (bad_hash)(const_0_hash)
#endif

#define vr_KV_GENS      (random_kv_gen)(inc_kv_gen)(dec_kv_gen)(inc128_kv_gen)(dec32_kv_gen)

//............................................................................

#define vr_SCENARIO(r, product) , scenario< BOOST_PP_SEQ_ENUM (product) >

using vr_SCENARIOS         = gt::Types
<
    scenario<default_chained_scatter_table, int32_t, /* 16-bit value */int16_t, crc32_hash, random_kv_gen>
    BOOST_PP_SEQ_FOR_EACH_PRODUCT (vr_SCENARIO, (vr_HASHTABLES)(vr_K_TYPES)(vr_V_TYPES)(vr_HASHES)(vr_KV_GENS))
>;

//............................................................................

template<typename T> struct vr_HASHTABLE_TEST: public gt::Test { };
TYPED_TEST_CASE (vr_HASHTABLE_TEST, vr_SCENARIOS);

#define vr_TYPED_TEST(case_name, test_name) TYPED_TEST (case_name, test_name)

//............................................................................
/*
 * fill table to capacity, erase all keys, repeat
 */
vr_TYPED_TEST (vr_HASHTABLE_TEST, fill_empty_cycle)
{
    using scenario          = TypeParam; // test parameter

    using table_type        = typename scenario::table_type;
    constexpr bool is_fixed_capacity    = table_type::is_fixed_capacity ();

    using kv_generator      = typename scenario::kv_generator;

    using key_type          = typename table_type::key_type;
    using value_type        = typename table_type::value_type;

    using size_type         = typename table_type::size_type;

    size_type const max_tested_capacity = VR_IF_THEN_ELSE (VR_DEBUG)(4096, std::min (32 * 1024, table_type::max_capacity ()));

    for (size_type const capacity : { /* edge case */1, 2, 128, max_tested_capacity })
    {
        LOG_info << "[capacity: " << capacity << ']';

        table_type t { (is_fixed_capacity ? capacity : 1) };

        // table empty when constructed:

        ASSERT_TRUE (t.empty ());
        t.check ();

        for (int32_t cycle = 0; cycle < 5; ++ cycle)
        {
            LOG_trace1 << "  [cycle " << cycle << ']';

            // fill with a sequence of unique keys:

            kv_generator kvg { test::env::random_seed<key_type> () };
            kv_generator kvg_replay { kvg };

            for (int32_t i = 0; i < capacity; ++ i)
            {
                ASSERT_EQ (t.size (), i);

                auto const kv = kvg ();

                key_type const & k = kv.first;
                value_type const & v = kv.second;

                ASSERT_FALSE (t.get (k)); // 'k' is not in the table

                value_type const * vi = t.put (k, v);
                ASSERT_TRUE (vi); // 'k' is now in the table
                EXPECT_EQ (* vi, v);

                ASSERT_EQ (t.size (), i + 1);

                vi = t.get (k); // find 'k' again
                ASSERT_TRUE (vi);
                EXPECT_EQ (* vi, v);
            }
            ASSERT_EQ (t.size (), capacity);
            ASSERT_FALSE (t.empty ());
            t.check ();

            if (table_type::is_fixed_capacity ())
            {
                // validate that the next insert will trigger a "capacity reached" exception:

                auto const kv = kvg ();
                ASSERT_THROW (t.put (kv.first, kv.second), capacity_limit);

                // 't' state should be unchanged:

                ASSERT_EQ (t.size (), capacity);
                t.check ();
            }

            // remove the sequence in the same order:

            for (int32_t i = 0; i < capacity; ++ i)
            {
                ASSERT_EQ (t.size (), capacity - i);

                auto const kv = kvg_replay ();

                key_type const & k = kv.first;
                value_type const & v = kv.second;

                value_type const * vi = t.get (k);
                ASSERT_TRUE (vi); // 'k' is still in the table
                EXPECT_EQ (* vi, v); // 'k' maps to 'v'

                vi = t.put (k, v / 2); // update value
                ASSERT_TRUE (vi);
                EXPECT_EQ (* vi, v / 2);

                bool rc = t.remove (k);
                ASSERT_TRUE (rc); // successful deletion

                ASSERT_EQ (t.size (), capacity - i - 1);

                vi = t.get (k); // search for 'k' again
                ASSERT_FALSE (vi);

                rc = t.remove (k); // delete 'k' again
                ASSERT_FALSE (rc); // unsuccessful deletion
            }
            ASSERT_EQ (t.size (), 0);
            ASSERT_TRUE (t.empty ());
            t.check ();
        }
    }
}
//............................................................................
/*
 * fill table to a given expected size, then do windowed inserts/deletions
 * to keep the size (roughly) steady
 */
vr_TYPED_TEST (vr_HASHTABLE_TEST, steady_state)
{
    using scenario          = TypeParam; // test parameter

    using table_type        = typename scenario::table_type;
    constexpr bool is_fixed_capacity    = table_type::is_fixed_capacity ();

    using kv_generator      = typename scenario::kv_generator;

    using key_type          = typename table_type::key_type;
    using value_type        = typename table_type::value_type;

    using size_type         = typename table_type::size_type;

    using check_map         = boost::unordered_map<key_type, value_type>;

    size_type const max_tested_capacity = VR_IF_THEN_ELSE (VR_DEBUG)(4096, std::min (32 * 1024, table_type::max_capacity ()));

    uint32_t urng { test::env::random_seed<uint32_t> () };

    for (size_type const capacity : { 128, max_tested_capacity })
    {
        for (size_type max_size : { capacity / 3, (3 * capacity) / 4, capacity } )
        {
            LOG_info << "[capacity: " << capacity << ", max size " << max_size << ']';

            table_type t { (is_fixed_capacity ? capacity : 1) };
            check_map cm { };

            // populate initially:

            kv_generator kvg { test::env::random_seed<key_type> () };
            kv_generator kvg_replay { kvg };

            for (int32_t i = 0; i < max_size; ++ i)
            {
                auto const kv = kvg ();

                key_type const & k = kv.first;
                value_type const & v = kv.second;

                t.put (k, v);
                cm.emplace (k, v);
            }
            ASSERT_EQ (t.size (), max_size);

            // ['t' and 'cm' have identical content]

            ASSERT_EQ (t.size (), cm.size ());

            // do the steady state pass:

            for (int32_t i = 0; i < VR_IF_THEN_ELSE (VR_DEBUG)(1000, 10000); ++ i)
            {
                int32_t const op_count = 1 + xorshift (urng) % std::max<int32_t> (5, cm.size ());

                for (int32_t j = 0; j < op_count; ++ j) // delete some oldest keys
                {
                    auto const kv = kvg_replay ();

                    key_type const & k = kv.first;

                    bool rc = t.remove (k);
                    ASSERT_TRUE (rc);

                    value_type const * const vi = t.get (k); // search for 'k' again
                    ASSERT_FALSE (vi);

                    rc = t.remove (k); // delete 'k' again
                    ASSERT_FALSE (rc); // unsuccessful deletion

                    cm.erase (k); // keep 'cm' in sync
                }

                for (int32_t j = 0; j < op_count; ++ j) // insert the same number of new keys
                {
                    auto const kv = kvg ();

                    key_type const & k = kv.first;
                    value_type const & v = kv.second;

                    value_type const * vi = t.put (k, v);
                    ASSERT_TRUE (vi); // 'k' is now in the table
                    ASSERT_EQ (* vi, v);

                    vi = t.get (k); // search for 'k' again
                    ASSERT_TRUE (vi);
                    ASSERT_EQ (* vi, v);

                    cm.emplace (k, v); // keep 'cm' in sync
                }
            }
            ASSERT_EQ (t.size (), max_size);
            t.check ();

            // 't' and 'cm' must still have identical content:

            ASSERT_EQ (t.size (), cm.size ());

            for (auto const & kv : cm)
            {
                value_type const * const tv = t.get (kv.first);
                ASSERT_TRUE (tv) << "key " << kv.first << " should be in 't'";
                ASSERT_EQ (* tv, kv.second) << "value " << print (* tv)<< " should be " << print (kv.second);
            }
        }
    }
}
//............................................................................
/*
 * do random inserts/deletions while keeping table size within some bounds,
 * validate against unordered_map kept in sync
 */
vr_TYPED_TEST (vr_HASHTABLE_TEST, random_mutation)
{
    using scenario          = TypeParam; // test parameter

    using table_type        = typename scenario::table_type;
    constexpr bool is_fixed_capacity    = table_type::is_fixed_capacity ();

    using kv_generator      = typename scenario::kv_generator;

    using key_type          = typename table_type::key_type;
    using value_type        = typename table_type::value_type;

    using size_type         = typename table_type::size_type;

    using check_map         = boost::unordered_map<key_type, value_type>;

    size_type const max_tested_capacity = VR_IF_THEN_ELSE (VR_DEBUG)(1024, std::min (32 * 1024, table_type::max_capacity ()));

    int32_t bool_rnd { test::env::random_seed<int32_t> () };
    key_type k_rnd { test::env::random_seed<key_type> () * 3 };

    for (size_type const capacity : { 32, 64, max_tested_capacity })
    {
        key_type key_range = std::min<key_type> (60, capacity);

        LOG_info << "[capacity: " << capacity << ", key range " << key_range << ']';

        table_type t { (is_fixed_capacity ? capacity : 1) };
        check_map cm { };

        // populate initially:
        {
            kv_generator kvg { test::env::random_seed<key_type> () };

            for (int32_t i = 0; i < capacity / 2; ++ i)
            {
                auto const kv = kvg ();

                key_type const & k = kv.first;
                value_type const & v = kv.second;

                t.put (k, v);
                cm.emplace (k, v);
            }
            ASSERT_EQ (t.size (), cm.size ());
        }

        // ['t' and 'cm' have identical content]

        // do the random I/O pass:

        for (int32_t i = 0; i < 100000; ++ i)
        {
            xorshift (bool_rnd);
            bool insert = (bool_rnd > 0);
            if (cm.size () >= capacity) insert = false;

            xorshift (k_rnd);
            key_type const k = k_rnd % key_range; // make sure we have some duplicates

            if (insert)
            {
                value_type const v = - k_rnd;

                auto const cmi = cm.emplace (k, v);

                if (cmi.second) // 'k' was not in the tables
                {
                    value_type const * tv = t.get (k);
                    ASSERT_FALSE (tv);
                    tv = t.put (k, v);
                    ASSERT_TRUE (tv);
                    ASSERT_EQ (* tv, v);
                }
                else // 'v' did not get inserted into 'cm'
                {
                    value_type const * const tv = t.get (k);
                    ASSERT_TRUE (tv);
                    ASSERT_EQ (* tv, cmi.first->second);
                }
            }
            else
            {
                auto const cme = cm.erase (k);

                if (cme) // 'k' was present and got deleted
                {
                    bool const te = t.remove (k);
                    ASSERT_TRUE (te);
                }
                else // 'k' was not present
                {
                    value_type const * tv = t.get (k);
                    ASSERT_FALSE (tv);
                }
            }
        }
        t.check ();

        // 't' and 'cm' must still have identical content:

        ASSERT_EQ (t.size (), cm.size ());

        for (auto const & kv : cm)
        {
            value_type const * const tv = t.get (kv.first);
            ASSERT_TRUE (tv) << "key " << kv.first << " should be in 't'";
            ASSERT_EQ (* tv, kv.second) << "value " << print (* tv)<< " should be " << print (kv.second);
        }
    }
}

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
