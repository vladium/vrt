#if 0 // TODO replace with __has_include testing for dense_hash_map presence

#include "vr/macros.h" // VR_RELEASE
#if VR_RELEASE // perf testcases in release builds only

#include "vr/containers/util/chained_scatter_table.h"
#include "vr/io/streams.h"
#include "vr/stats/stream_stats.h"
#include "vr/sys/cpu.h"
#include "vr/util/env.h"
#include "vr/util/logging.h"

#include "vr/test/timing.h"
#include "vr/test/utility.h"

#include <google/dense_hash_map>

#include <algorithm>
#include <cmath>
#include <unordered_map>

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
using namespace sys;

//............................................................................
//............................................................................
namespace
{
//............................................................................

VR_ENUM (table_kind,
    (
        std_unordered,
        google_dense,
        vlad_CS
    ),
    iterable, printable

); // end of enum
//............................................................................

// calculate a useful range of target sizes based on 'K', 'V', and cache info:

std::vector<std::size_t>
test_size_range (std::size_t const K_size, std::size_t const V_size, cpu_info const & ci)
{
    // the useful payload is 'K_size + V_size'; give benefit of the doubt
    // by using '2 * max (K_size, V_size)':

    std::size_t const entry_size = 2 * std::max (K_size, V_size);

    std::vector<std::size_t> r;

    r.push_back (128); // a tiny size

    std::size_t lvl_size { };
    for (int32_t lvl = L1; lvl <= ci.cache_info ().LLC (); ++ lvl)
    {
        lvl_size = ci.cache_info ().level_info (lvl).m_size;

        r.push_back (lvl_size / (2 * entry_size)); // ~ 50% of this level cache
        r.push_back (lvl_size / entry_size); // ~ 100% of this level cache
        r.push_back ((2 * lvl_size) / entry_size); // 2x of this level cache
    }

    return r;
}

// log-uniform in [start, 1.0):

std::vector<double>
test_alpha_range (double const start, int32_t const count, bool const allow_full)
{
    check_positive (count);

    std::vector<double> r (count);

    double const log_step = - std::log (start) / (count - 1 + ! allow_full);

    r [0] = start;
    for (int32_t i = /* ! */1; i < count; ++ i)
        r [i] = start * std::exp (i * log_step); // last value will be set to exact 1.0 of 'allow_full'

    if (allow_full) r.back () = 1.0;

    return r;
}

inline std::size_t
target_bucket_count (std::size_t const target_size, double const target_alpha)
{
    check_positive (target_alpha);

    return std::round (target_size / target_alpha);
}

std::vector<double> const qp { 0.25, 0.5, 0.75, 0.99 };

//............................................................................

std::ostream &
report_out ()
{
    static io::fd_ostream<> g_out { getenv<fs::path> ("VR_REPORT") };
    static bool g_header { false };

    if (! g_header)
    {
        g_out << "table,op,size,load_factor,median,worst,var" << std::endl;

        g_header = true;
    }


    return g_out;
}

using stats_type        = stats::stream_stats<int64_t>;

void
report (string_vector const & tags, stats_type const & ss, int64_t const overhead)
{
    std::ostream & out = report_out ();

    bool const have_tags = ! tags.empty ();

    if (have_tags)
    {
        for (std::string const & tag : tags)
        {
            out << tag << ",";
        }
    }

    out << static_cast<int64_t> (ss [1] - overhead) << ","
        << static_cast<int64_t> (ss [3] - overhead) << ","
        << std::setprecision (3) << ((ss [2] - ss [0]) / ss [1]);

    out << std::endl;
}
//............................................................................

template<typename K, typename V>
struct content_traits
{
    using key_type      = K;
    using value_type    = V;

}; // end of traits
//............................................................................

template<table_kind::enum_t TABLE_KIND, typename CONTENT_TRAITS>
struct table_ops; // master

// specializations:

template<typename CONTENT_TRAITS>
struct table_ops<table_kind::std_unordered, CONTENT_TRAITS>
{
    static constexpr table_kind::enum_t kind () { return table_kind::std_unordered; }

    using key_type      = typename CONTENT_TRAITS::key_type;
    using value_type    = typename CONTENT_TRAITS::value_type;

    using table_type    = std::unordered_map<key_type, value_type>;

    table_ops (std::size_t const size) :
        m_map { }
    {
        m_map.reserve (size);
        LOG_trace2 << "chosen bucket count: " << m_map.bucket_count ();
    }

    double alpha () const
    {
        return m_map.load_factor ();
    }

    VR_FORCEINLINE value_type const * get (key_type const & k) const
    {
        auto const i = m_map.find (k);
        return (i == m_map.end () ? nullptr : & i->second);
    }

    VR_FORCEINLINE void put (key_type const & k, value_type const & v)
    {
        m_map.emplace (k, v);
    }

    VR_FORCEINLINE bool remove (key_type const & k)
    {
        return m_map.erase (k);
    }


    table_type m_map;

}; // end of specialization


template<typename CONTENT_TRAITS>
struct table_ops<table_kind::google_dense, CONTENT_TRAITS>
{
    static constexpr table_kind::enum_t kind () { return table_kind::google_dense; }

    using key_type      = typename CONTENT_TRAITS::key_type;
    using value_type    = typename CONTENT_TRAITS::value_type;

    using table_type    = google::dense_hash_map<key_type, value_type, std::hash<key_type> >;

    table_ops (std::size_t const size) :
        m_map { size }
    {
        m_map.set_empty_key (0); // NOTE: the tests below use only non-zero keys
        m_map.set_deleted_key (std::numeric_limits<key_type>::min ());

        LOG_trace2 << "chosen bucket count: " << m_map.bucket_count ();
    }

    double alpha () const
    {
        return m_map.load_factor ();
    }

    VR_FORCEINLINE value_type const * get (key_type const & k) const
    {
        auto const i = m_map.find (k);
        return (i == m_map.end () ? nullptr : & i->second);
    }

    VR_FORCEINLINE void put (key_type const & k, value_type const & v)
    {
        m_map.insert (typename table_type::value_type { k, v });
    }

    VR_FORCEINLINE bool remove (key_type const & k)
    {
        return m_map.erase (k);
    }

    table_type m_map;

}; // end of specialization


template<typename CONTENT_TRAITS>
struct table_ops<table_kind::vlad_CS, CONTENT_TRAITS>
{
    static constexpr table_kind::enum_t kind () { return table_kind::vlad_CS; }

    using key_type      = typename CONTENT_TRAITS::key_type;
    using value_type    = typename CONTENT_TRAITS::value_type;

    using table_type    = chained_scatter_table<key_type, value_type>;

    table_ops (std::size_t const size, bool const choose_capacity = true) :
        m_map { static_cast<typename table_type::size_type> (size) }
    {
        LOG_trace2 << "chosen bucket count: " << m_map.capacity ();
    }

    double alpha () const
    {
        return (static_cast<double> (m_map.size ()) / m_map.capacity ());
    }

    VR_FORCEINLINE value_type const * get (key_type const & k) const
    {
        return m_map.get (k);
    }

    VR_FORCEINLINE void put (key_type const & k, value_type const & v)
    {
        m_map.put (k, v);
    }

    VR_FORCEINLINE bool remove (key_type const & k)
    {
        return m_map.remove (k);
    }

    table_type m_map;

}; // end of specialization

}
//............................................................................
//............................................................................

// TODO add perf API instrumentation

using table_kinds       = gt::Types
<
    enum_<table_kind, table_kind::std_unordered>,
    enum_<table_kind, table_kind::google_dense>,
    enum_<table_kind, table_kind::vlad_CS>
>;

template<typename T> struct perf_hashtable_test: gt::Test {};
TYPED_TEST_CASE (perf_hashtable_test, table_kinds);


TYPED_TEST (perf_hashtable_test, get) // [steady state] 'get()' hit/miss
{
    using scenario      = TypeParam; // testcase parameter

    cpu_info const & ci = cpu_info::instance ();
    int64_t dummy { };

    int32_t const pu                    = 2; // stay away from core #0
    std::size_t const min_sample_count  = 10000000;
    bool const keep_L1d_hot             = true;

    using kv_traits                 = content_traits<int32_t, int64_t>;
    table_kind::enum_t const kind   = scenario::value;

    using key_type      = typename kv_traits::key_type;
    using value_type    = typename kv_traits::value_type;

    using ops           = table_ops<kind, kv_traits>;

    LOG_info << "table kind: " << print (kind) << ", hot L1d: " << keep_L1d_hot;

    std::vector<std::size_t> const sizes = test_size_range (sizeof (key_type), sizeof (value_type), ci);
    LOG_info << "table sizes: " << print (sizes);

    {
        affinity::scoped_thread_sched _ { make_bit_set (ci.PU_count ()).set (pu) }; // pin to 'pu'


        int64_t const overhead = test::tsc_macro_overhead ();
        LOG_info << "TSC measurement overhead: " << overhead << " cycle(s)";

        key_type rnd = test::env::random_seed<key_type> (); // note: identical seed for all tests run in the same process
        int64_t tsc;

        for (int32_t pass = 0; pass < 2; ++ pass)
        {
            LOG_info << "------------------------------------------";
            LOG_info << "PASS " << (pass + 1) << " (" << (pass ? "measurement" : "warm-up") << ')';
            LOG_info << "------------------------------------------";

            for (std::size_t const size : sizes)
            {
                stats_type ss_hit { qp }, ss_miss { qp };

                test::flush_cpu_cache (sys::L3); // get the data cache into a somewhat predictable state TODO if requested

                ops t { size };

                // (1) populate 't' with 'size' "unique" positive keys (some chance of duplicates due to abs()):

                key_type const rnd_c { rnd }; // for replay later

                for (std::size_t i = 0; i < size; ++ i)
                {
                    key_type const k = std::abs (util::xorshift (rnd));

                    t.put (k, - k);
                }

                std::size_t const rounds = (pass ? (min_sample_count + size - 1) / size : 1);
                LOG_info << "  size: " << size << ", alpha: " << std::setprecision (3) << t.alpha () << ", rounds: " << rounds;

                // (2) get[hit] pass:
                {
                    for (std::size_t round = 0; round < rounds; ++ round)
                    {
                        key_type rnd_replay = rnd_c;

                        for (std::size_t i = 0; i < size; ++ i)
                        {
                            if (! keep_L1d_hot) test::flush_cpu_cache (sys::L1);

                            key_type const k = std::abs (util::xorshift (rnd_replay));

                            value_type const * v;
                            {
                                tsc = VR_TSC_START ();
                                {
                                    v = t.get (k);
                                }
                                VR_TSC_STOP (tsc);
                                ss_hit (tsc);
                            }
                            assert_nonnull (v);
                            assert_eq ((* v), -k);

                            dummy += (* v);
                        }
                    }
                }

                // (3) get[miss] pass:
                {
                    for (std::size_t round = 0; round < rounds; ++ round)
                    {
                        key_type rnd_replay = rnd_c;

                        for (std::size_t i = 0; i < size; ++ i)
                        {
                            if (! keep_L1d_hot) test::flush_cpu_cache (sys::L1);

                            key_type const k = - std::abs (util::xorshift (rnd_replay)); // any negative key will miss

                            value_type const * v;
                            {
                                tsc = VR_TSC_START ();
                                {
                                    v = t.get (k);
                                }
                                VR_TSC_STOP (tsc);
                                ss_miss (tsc);
                            }
                            assert_null (v);

                            dummy += reinterpret_cast<std_uintptr_t> (v);
                        }
                    }
                }

                if (pass)
                {
                    report ({ print (kind), print ("get.hit"),  print (size), print (t.alpha ()) }, ss_hit, overhead);
                    report ({ print (kind), print ("get.miss"), print (size), print (t.alpha ()) }, ss_miss, overhead);
                }
            }
        }
    }

    LOG_trace3 << "dummy: " << dummy; // prevent compiler from eliding 'v'
}
//............................................................................

TYPED_TEST (perf_hashtable_test, put_remove) // [steady state] remove hit followed by put miss
{
    using scenario      = TypeParam; // testcase parameter

    cpu_info const & ci = cpu_info::instance ();
    int64_t dummy { };

    int32_t const pu                    = 2; // stay away from core #0
    std::size_t const min_sample_count  = 10000000;
    bool const keep_L1d_hot             = true;

    using kv_traits                 = content_traits<int32_t, int64_t>;
    table_kind::enum_t const kind   = scenario::value;

    using key_type      = typename kv_traits::key_type;
    using value_type    = typename kv_traits::value_type;

    using ops           = table_ops<kind, kv_traits>;

    LOG_info << "table kind: " << print (kind) << ", hot L1d: " << keep_L1d_hot;

    std::vector<std::size_t> const sizes = test_size_range (sizeof (key_type), sizeof (value_type), ci);
    LOG_info << "table sizes: " << print (sizes);

    {
        affinity::scoped_thread_sched _ { make_bit_set (ci.PU_count ()).set (pu) }; // pin to 'pu'


        int64_t const overhead = test::tsc_macro_overhead ();
        LOG_info << "TSC measurement overhead: " << overhead << " cycle(s)";

        key_type rnd = test::env::random_seed<key_type> (); // note: identical seed for all tests run in the same process
        int64_t tsc;

        for (int32_t pass = 0; pass < 2; ++ pass)
        {
            LOG_info << "------------------------------------------";
            LOG_info << "PASS " << (pass + 1) << " (" << (pass ? "measurement" : "warm-up") << ')';
            LOG_info << "------------------------------------------";

            for (std::size_t const size : sizes)
            {
                stats_type ss_remove { qp }, ss_put { qp };

                test::flush_cpu_cache (sys::L3); // get the data cache into a somewhat predictable state TODO if requested

                ops t { size };

                // (1) populate 't' with 'size' unique positive keys:

                key_type const rnd_c { rnd }; // for lagged rnd sequence below

                for (std::size_t i = 0; i < size; ++ i)
                {
                    key_type const k = util::xorshift (rnd);

                    t.put (k, - k);
                }

                std::size_t const i_limit = (pass ? min_sample_count : size);
                LOG_info << "  size: " << size << ", alpha: " << std::setprecision (3) << t.alpha () << ", samples: " << i_limit;

                // (2) remove[hit], put[miss] pass:
                {
                    key_type rnd_lagged = rnd_c;

                    for (std::size_t i = 0; i < i_limit; ++ i)
                    {
                        if (! keep_L1d_hot) test::flush_cpu_cache (sys::L1);

                        key_type const k_remove = util::xorshift (rnd_lagged);
                        key_type const k_put = util::xorshift (rnd);

                        bool rc;
                        {
                            tsc = VR_TSC_START ();
                            {
                                rc = t.remove (k_remove);
                            }
                            VR_TSC_STOP (tsc);
                            ss_remove (tsc);
                        }
                        assert_condition (rc, i);
                        dummy += rc;

                        {
                            tsc = VR_TSC_START ();
                            {
                                t.put (k_put, - k_put);
                            }
                            VR_TSC_STOP (tsc);
                            ss_put (tsc);
                        }
                    }
                }

                if (pass)
                {
                    report ({ print (kind), print ("put"),    print (size), print (t.alpha ()) }, ss_put, overhead);
                    report ({ print (kind), print ("remove"), print (size), print (t.alpha ()) }, ss_remove, overhead);
                }
            }
        }
    }

    LOG_trace3 << "dummy: " << dummy; // prevent compiler from eliding 'v'
}
//............................................................................

using vr_table_kinds       = gt::Types
<
    enum_<table_kind, table_kind::vlad_CS>,
    enum_<table_kind, table_kind::vlad_RH>
>;

template<typename T> struct perf_vr_hashtable_test: gt::Test {};
TYPED_TEST_CASE (perf_vr_hashtable_test, vr_table_kinds);

TYPED_TEST (perf_vr_hashtable_test, put_remove_vs_alpha) // [steady state] mutators as fns of load factor
{
    using scenario      = TypeParam; // testcase parameter

    cpu_info const & ci = cpu_info::instance ();
    int64_t dummy { };

    int32_t const pu                    = 2; // stay away from core #0
    std::size_t const min_sample_count  = 10000000;
    bool const keep_L1d_hot             = true;

    using kv_traits                 = content_traits<int32_t, int64_t>;
    table_kind::enum_t const kind   = scenario::value;

    using key_type      = typename kv_traits::key_type;

    using ops           = table_ops<kind, kv_traits>;

    LOG_info << "table kind: " << print (kind) << ", hot L1d: " << keep_L1d_hot;

    std::size_t const capacity  = 4096;

    std::vector<std::size_t> sizes;
    {
        std::vector<double> const alphas = test_alpha_range (0.5, 30, false);
        for (double alpha : alphas) sizes.push_back (std::round (capacity * alpha));
    }
    LOG_info << "table sizes: " << print (sizes);

    {
        affinity::scoped_thread_sched _ { make_bit_set (ci.PU_count ()).set (pu) }; // pin to 'pu'


        int64_t const overhead = test::tsc_macro_overhead ();
        LOG_info << "TSC measurement overhead: " << overhead << " cycle(s)";

        key_type rnd = test::env::random_seed<key_type> (); // note: identical seed for all tests run in the same process
        int64_t tsc;

        for (int32_t pass = 0; pass < 2; ++ pass)
        {
            LOG_info << "------------------------------------------";
            LOG_info << "PASS " << (pass + 1) << " (" << (pass ? "measurement" : "warm-up") << ')';
            LOG_info << "------------------------------------------";

            for (std::size_t const size : sizes)
            {
                stats_type ss_remove { qp }, ss_put { qp };

                test::flush_cpu_cache (sys::L3); // get the data cache into a somewhat predictable state TODO if requested

                ops t { capacity, false }; // note the diff with earlier tests

                // (1) populate 't' with 'size' unique positive keys:

                key_type const rnd_c { rnd }; // for lagged rnd sequence below

                for (std::size_t i = 0; i < size; ++ i)
                {
                    key_type const k = util::xorshift (rnd);

                    t.put (k, - k);
                }

                std::size_t const i_limit = (pass ? min_sample_count : size);
                LOG_info << "  size: " << size << ", alpha: " << std::setprecision (3) << t.alpha () << ", samples: " << i_limit;

                // (2) remove[hit], put[miss] pass:
                {
                    key_type rnd_lagged = rnd_c;

                    for (std::size_t i = 0; i < i_limit; ++ i)
                    {
                        if (! keep_L1d_hot) test::flush_cpu_cache (sys::L1);

                        key_type const k_remove = util::xorshift (rnd_lagged);
                        key_type const k_put = util::xorshift (rnd);

                        bool rc;
                        {
                            tsc = VR_TSC_START ();
                            {
                                rc = t.remove (k_remove);
                            }
                            VR_TSC_STOP (tsc);
                            ss_remove (tsc);
                        }
                        assert_condition (rc, i);
                        dummy += rc;

                        {
                            tsc = VR_TSC_START ();
                            {
                                t.put (k_put, - k_put);
                            }
                            VR_TSC_STOP (tsc);
                            ss_put (tsc);
                        }
                    }
                }

                if (pass)
                {
                    report ({ print (kind), print ("put"),    print (size), print (t.alpha ()) }, ss_put, overhead);
                    report ({ print (kind), print ("remove"), print (size), print (t.alpha ()) }, ss_remove, overhead);
                }
            }
        }
    }

    LOG_trace3 << "dummy: " << dummy; // prevent compiler from eliding 'v'
}

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
#endif // VR_RELEASE
#endif
