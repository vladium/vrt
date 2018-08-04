
#include "vr/containers/util/chained_scatter_table.h"

#include "vr/test/utility.h"

#include <boost/preprocessor/iteration/iterate.hpp>

#include <ratio>

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
//............................................................................
//............................................................................

// TODO
// - iterator checks
template<typename K, typename V, typename HASH, typename OPTIONS>
void
chained_scatter_table<K, V, HASH, OPTIONS>::check () const
{
    // traverse all buckets, sorting them into occupied and free slots:

    using addr_set      = boost::unordered_set<link_type>;

    size_type const data_bucket_count { capacity () };
    check_is_power_of_2 (data_bucket_count); // implies is positive also

    check_condition (m_buckets);
    entry_slot * VR_RESTRICT const buckets = m_buckets.get ();

    addr_set free_entries { };
    addr_set occupied_entries { };

    for (link_type addr =/* ! */1; addr /* ! */<= data_bucket_count; ++ addr) // this doesn't visit the sentinel slot
    {
        entry_slot const & e = buckets [addr];
        auto const & e_next = field<_next_> (e.free);

        if (e_next < 0)
            free_entries.insert (addr);
        else
            occupied_entries.insert (addr);
    }
    check_eq (signed_cast (occupied_entries.size ()), size ()); // 'm_size' count is correct

    // validate that iteration finds the same set of occupied entries:

    size_type i_count { };
    for (auto const & ue : (* this))
    {
        link_type const addr = (reinterpret_cast<entry_slot const *> (& ue) - & buckets [0]);
        check_eq (occupied_entries.count (addr), 1);

        ++ i_count;
    }
    check_eq (i_count, size ());

    // validate that free list is a simple single cycle (when the sentinel is included) in both link directions:

    entry_slot const & sentinel = buckets [0];

    LOG_trace2 << "  validating free list (" << free_entries.size () << " slot(s))";

    if (free_entries.empty ())
    {
        // sentinel points to itself, both ways:

        check_eq (field<_next_> (sentinel.free), - 1);
        check_eq (field<_prev_> (sentinel.free), 0);
    }
    else
    {
        // 'next':
        {
            addr_set unvisited { free_entries };

            link_type addr = - field<_next_> (sentinel.free) - 1;
            check_condition (free_entries.count (addr), addr); // sentinel's 'next' points to a free slot

            entry_slot const * e = & buckets [addr]; // sentinel's successor

            for (int32_t i = 0, i_limit = free_entries.size (); i < i_limit; ++ i) // guard against unexpected cycles
            {
                check_condition (unvisited.erase (addr));

                addr = - field<_next_> (e->free) - 1;
                check_nonnegative (addr);

                e = & buckets [addr];
            }
            // [assertion: 'unvisited' has been emptied and 'e' is the sentinel]

            check_empty (unvisited, unvisited.size ());
            check_eq (e, & sentinel);
        }
        // 'prev':
        {
            addr_set unvisited { free_entries };

            link_type addr = field<_prev_> (sentinel.free);
            check_condition (free_entries.count (addr), addr); // sentinel's 'prev' points to a free slot

            entry_slot const * e = & buckets [addr]; // sentinel's predecessor

            for (int32_t i = 0, i_limit = free_entries.size (); i < i_limit; ++ i) // guard against unexpected cycles
            {
                check_condition (unvisited.erase (addr));

                addr = field<_prev_> (e->free);
                check_nonnegative (addr);

                e = & buckets [addr];
            }
            // [assertion: 'unvisited' has been emptied and 'e' is the sentinel]

            check_empty (unvisited, unvisited.size ());
            check_eq (e, & sentinel);
        }
    }

    // validate that occupied nodes are organized in chains that are:
    //  - simple lists (linked via 'next') terminated by a null link;
    //  - are disjoint (no coalescing);
    //  - correspond to primary collisions only (the same hash addr).
    //
    // furthermore, all keys are (globally) unique

    using key_set       = boost::unordered_set<key_type, HASH>;

    // identify all slots that are the heads of collision chains:

    using haddr_map     = boost::unordered_map<link_type, usize_type>;

    haddr_map chains { };
    {
        key_set uniq_keys { };

        for (link_type addr : occupied_entries)
        {
            entry_slot const & e = buckets [addr];
            auto const & e_key = field<_key_> (e.used);

            check_condition (uniq_keys.insert (e_key).second); // global key uniqueness

            usize_type const e_key_haddr { (static_cast<usize_type> (HASH { } (e_key)) & m_capacity_mask) +/* sentinel */1 };

            if (static_cast<link_type> (e_key_haddr) == addr) chains.emplace (addr, e_key_haddr);
        }
    }
    LOG_trace2 << "  table has " << chains.size () << " chain(s) for " << size () << " key(s)";

    bool const trace3 = LOG_trace_enabled (3);

    for (auto const & kv : chains) // note: by construction, every chain contains at least one node
    {
        entry_slot const * e = & buckets [kv.first];

        std::stringstream ss { };

        auto const & e_head_key = field<_key_> (e->used);
        usize_type const e_head_key_haddr { (static_cast<usize_type> (HASH { } (e_head_key)) & m_capacity_mask) +/* sentinel */1 };

        if (trace3) ss << "    chain: #" << e_head_key_haddr << " <" << print (e_head_key) << ':' << print (field<_value_> (* e)) << '>';

        while (field<_next_> (e->used))
        {
            e = & buckets [field<_next_> (e->used)];

            auto const & e_key = field<_key_> (e->used);
            usize_type const e_key_haddr { (static_cast<usize_type> (HASH { } (e_key)) & m_capacity_mask) +/* sentinel */1 };

            if (trace3) ss << " => #" << (e - buckets) << " <" << print (e_key) << ':' << print (field<_value_> (* e)) << '>';

            check_eq (e_key_haddr, e_head_key_haddr, e_key, e_head_key);
        }

        LOG_trace3 << ss.str ();
    }
}
//............................................................................

template<typename K, typename V, typename HASH>
using default_chained_scatter_table         = chained_scatter_table<K, V, HASH>;

template<typename K, typename V, typename HASH>
using fixed_chained_scatter_table           = chained_scatter_table<K, V, HASH, chained_scatter_table_options<fixed_chained_scatter_table_traits> >;

//............................................................................
//............................................................................

// note: not using an anonymous ns to keep testcase names shorter

template<typename T, std::size_t V, typename V_PROBABILITY, typename HASH>
struct bad_hash_impl // output 'V' with given probability, otherwise delegate to 'HASH'
{
    vr_static_assert (std::is_default_constructible<HASH>::value);

    std::size_t operator() (T const & obj) const VR_NOEXCEPT
    {
        if ((V_PROBABILITY::num != 0) && (test::next_random<int64_t, double> (m_rnd) <= static_cast<double> (V_PROBABILITY::num) / V_PROBABILITY::den))
            return V;
        else
            return HASH { } (obj);
    }

    int64_t mutable m_rnd { test::env::random_seed<int64_t> () };

}; // end of class

template<typename T>
using bad_hash              = bad_hash_impl<T, 19, std::ratio<1, 4>, crc32_hash<T> >;


template<typename T, std::size_t V>
struct const_hash_impl // arguably, the "worst" hash
{
    std::size_t operator() (T const & obj) const VR_NOEXCEPT
    {
        return V;
    }

}; // end of class

template<typename T>
using const_0_hash          = const_hash_impl<T, 0>;

//............................................................................

template<typename K, typename V, int32_t STEP>
struct monotonic_kv_gen final // unique as long as there's no 'K' wraparound
{
    monotonic_kv_gen (K const seed)
    {
    }

    std::pair<K, V> operator() () VR_NOEXCEPT
    {
        return { m_k += STEP, m_v -= STEP };
    }

    K m_k { (STEP > 0 ? std::numeric_limits<K>::min () : std::numeric_limits<K>::max ()) };
    V m_v { (STEP > 0 ? std::numeric_limits<V>::max () : std::numeric_limits<V>::min ()) }; // goes in the opposite direction from 'm_k'

}; // end of class

template<typename K, typename V>
using inc_kv_gen            = monotonic_kv_gen<K, V, +1>;

template<typename K, typename V>
using dec_kv_gen            = monotonic_kv_gen<K, V, -1>;

template<typename K, typename V>
using inc128_kv_gen         = monotonic_kv_gen<K, V, +128>; // force primary collisions for some capacities and hashes

template<typename K, typename V>
using dec32_kv_gen          = monotonic_kv_gen<K, V, -32>; // force primary collisions for some capacities and hashes


template<typename K, typename V>
struct random_kv_gen final // note: random, but unique as long as there's no 'K' truncation/wraparound
{
    random_kv_gen (K const seed) :
        m_rnd { seed }
    {
    }

    std::pair<K, V> operator() () VR_NOEXCEPT
    {
        K const k = xorshift (m_rnd);
        V const v = static_cast<V> (- k) * 10; // value easily derivable from key for easier testing

        return { k, v };
    }

    K m_rnd;

}; // end of class
//............................................................................

template
<
    template<typename /* K */, typename /* V */, typename /* HASH */> class TABLE_IMPL,
    typename K, typename V,
    template<typename /* K */> class HASH,
    template<typename /* K */, typename /* V */> class KV_GENERATOR
>
struct scenario
{
    using hasher                            = HASH<K>;
    using table_type                        = TABLE_IMPL<K, V, hasher>;

    using kv_generator                      = KV_GENERATOR<K, V>;

}; // end of scenario

using int_ops       = ops_int<arg_policy<zero_arg_policy::ignore, 0>, true>;

//............................................................................

TEST (hashtable_test, iterate)
{
    using key_type      = int32_t;
    using value_type    = int64_t;

    using table         = chained_scatter_table<key_type, value_type, crc32_hash<key_type> >;

    constexpr table::size_type capacity = 512;

    // empty table:
    {
        table t { capacity };
        table const & t_const = t;

        table::size_type count { };
        for (auto i = t_const.begin (); i != t_const.end (); ++ i) // const iterators
        {
            ++ count;
        }
        ASSERT_EQ (count, 0);
    }

    // non-empty table, sequential key values:

    for (table::size_type sz : { static_cast<table::size_type> (1), 2, capacity / 3, (3 * capacity) / 4, capacity - 1, capacity })
    {
        table t { capacity };

        key_type const k_start = test::env::random_seed<key_type> () + sz;

        for (key_type k = k_start; k < k_start + sz; ++ k)
        {
            t.put (k, static_cast<value_type> (10) * k);
        }
        ASSERT_EQ (t.size (), sz);

        t.check ();

        table::size_type count { };
        for (auto const & e : t) // non-const iterators
        {
            key_type const & k = field<_key_> (e);
            value_type const & v = field<_value_> (e);

            ASSERT_EQ (v, static_cast<value_type> (10) * k) << "[sz: " << sz << "] invalid entry visited: k = " << k << ", v = " << v;

            ++ count;
        }
        ASSERT_EQ (count, t.size ()) << "[sz: " << sz << "]";
    }

    // non-empty table, randomized key values:

    key_type seed = test::env::random_seed<key_type> () * 3;

    for (table::size_type sz : { static_cast<table::size_type> (1), 2, capacity / 7, (3 * capacity) / 5, capacity - 1, capacity })
    {
        table t { capacity };

        key_type k = seed;
        for (int32_t i = 0; i < sz; ++ i)
        {
            t.put (k, static_cast<value_type> (10) * k);
            test::next_random (k);
        }
        ASSERT_EQ (t.size (), sz);

        t.check ();

        table::size_type count { };
        for (auto const & e : t) // non-const iterators
        {
            key_type const & k = field<_key_> (e);
            value_type const & v = field<_value_> (e);

            ASSERT_EQ (v, static_cast<value_type> (10) * k) << "[sz: " << sz << "] invalid entry visited: k = " << k << ", v = " << v;

            ++ count;
        }
        ASSERT_EQ (count, t.size ()) << "[sz: " << sz << "]";
    }
}
//............................................................................
/*
 * other tests check the logic of increasing capacity exhaustively, this one
 * checks correctness of a user-invoked downsizing rehash:
 */
TEST (hashtable_test, rehash_downsize)
{
    using key_type      = int32_t;
    using value_type    = int64_t;

    using table         = chained_scatter_table<key_type, value_type, crc32_hash<key_type> >;

    constexpr table::size_type max_size = 1024;

    constexpr table::size_type size_A = (3 * max_size / 4);
    constexpr table::size_type size_B = (1 * max_size / 4);

    key_type const seed = test::env::random_seed<key_type> ();

    table t { 1 };

    // insert entries A:

    key_type k = seed;

    for (int32_t i = 0; i < size_A; ++ i)
    {
        t.put (k, static_cast<value_type> (10) * k);
        test::next_random (k);
    }

    key_type const k_middle = k; // to be able to repeat 2nd half of rnd sequence

    // insert entries B:

    for (int32_t i = 0; i < size_B; ++ i)
    {
        t.put (k, static_cast<value_type> (10) * k);
        test::next_random (k);
    }
    ASSERT_EQ (t.size (), max_size);

    // erase entries A:

    k = seed;

    for (int32_t i = 0; i < size_A; ++ i)
    {
        t.remove (k);
        test::next_random (k);
    }

    t.check ();

    // rehash to compact:

    t.rehash (t.size ());

    // verify that entries B are still there:

    t.check ();

    ASSERT_EQ (t.size (), size_B);

    k = k_middle;

    for (int32_t i = 0; i < size_B; ++ i)
    {
        value_type const * const v = t.get (k);
        ASSERT_TRUE (v != nullptr);
        ASSERT_EQ (* v, static_cast<value_type> (10) * k);

        test::next_random (k);
    }
}

} // end of 'util'
} // end of namespace
//............................................................................

#if VR_FULL_TESTS // currently a build-time option only, not run-time

// release the kraken:

#define BOOST_PP_ITERATION_PARAMS_1 (3, (1, 2, "vr/containers/util/hashtable_test.inc"))
#include BOOST_PP_ITERATE ()

#endif // VR_FULL_TESTS
//----------------------------------------------------------------------------
