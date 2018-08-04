
#include "vr/sys/cpu.h"

#include "vr/strings.h"
#include "vr/util/logging.h"
#include "vr/mc/mc.h"
#include "vr/util/memory.h"
#include "vr/util/singleton.h"

#include <hwloc.h>

//----------------------------------------------------------------------------
namespace vr
{
namespace sys
{
//............................................................................

struct cpu_info::state final: private noncopyable
{
    state () :
        m_obj_counts (hw_obj::size) // note: vector construction
    {
        m_cache.m_impl = & m_cache_data;
    }

    using cache_data_all_types      = std::array<cache::level, cache_type::size>;

    ::hwloc_topology_t m_hw_topo { nullptr };
    std::vector<int32_t> m_obj_counts;
    std::vector<cache_data_all_types> m_cache_data;
    cache m_cache { }; // pimpl-like wrapper around 'm_cache_data'

}; // end of nested class
//............................................................................
//............................................................................

int32_t
cpu_info::cache::level_count () const
{
    assert_nonnull (m_impl);
    std::vector<state::cache_data_all_types> const & cache_data = * static_cast<std::vector<state::cache_data_all_types> const *> (m_impl);

    return cache_data.size ();
}

cpu_info::cache::level const &
cpu_info::cache::level_info (int32_t const lvl, cache_type::enum_t const ct) const
{
    assert_nonnull (m_impl);
    std::vector<state::cache_data_all_types> const & cache_data = * static_cast<std::vector<state::cache_data_all_types> const *> (m_impl);

    check_in_inclusive_range (lvl, 1, cache_data.size ());

    return cache_data [lvl - 1][ct];
}
//............................................................................
//............................................................................

template<>
struct cpu_info::access_by<void> final : public util::singleton_constructor<cpu_info>
{
    access_by (cpu_info * const obj)
    {
        auto const hw_api_v = ::hwloc_get_api_version ();
        LOG_trace1 << "initializing hwloc v" << hex_string_cast (hw_api_v) << " (built against v"  VR_TO_STRING (HWLOC_API_VERSION) ")";

#   if HWLOC_API_VERSION >= 0x00020000 // built against v2.x
        if (hw_api_v < 0x20000)
            throw_x (sys_exception, "hwloc runtime library is older () than v2.0");
#   else
        if (hw_api_v >= 0x20000)
            throw_x (sys_exception, "hwloc runtime library is newer () than version built against ()");
#   endif

        new (obj) cpu_info { };
    }

}; // end of class
//............................................................................
//............................................................................

using create_cpu_info       = cpu_info::access_by<void>;

cpu_info const &
cpu_info::instance ()
{
    return util::singleton<cpu_info, create_cpu_info>::instance ();
}
//............................................................................

cpu_info::cpu_info () :
    m_state { new state { } }

{
    ::hwloc_topology_t & hw_topo = m_state->m_hw_topo;

    VR_CHECKED_SYS_CALL (::hwloc_topology_init (& hw_topo));

    bitset64_t hw_flags = ::hwloc_topology_get_flags (hw_topo);
    hw_flags |= HWLOC_TOPOLOGY_FLAG_ICACHES;
    VR_CHECKED_SYS_CALL (::hwloc_topology_set_flags (hw_topo, hw_flags)); // note: must be done before topology load

    VR_CHECKED_SYS_CALL (::hwloc_topology_load (hw_topo));

    // coarse structure (object counts):

    for (hw_obj::enum_t t : hw_obj::values ())
    {
        ::hwloc_obj_type_t hw_t { };

        switch (t)
        {
            case hw_obj::PU:        hw_t = HWLOC_OBJ_PU; break;
            case hw_obj::core:      hw_t = HWLOC_OBJ_CORE; break;
            case hw_obj::socket:    hw_t = HWLOC_OBJ_PACKAGE; break;
            case hw_obj::node:      hw_t = HWLOC_OBJ_NUMANODE; break;

            default: VR_ASSUME_UNREACHABLE ();

        } // end of switch

        m_state->m_obj_counts [t] = ::hwloc_get_nbobjs_by_type (hw_topo, hw_t);
        if (VR_UNLIKELY (m_state->m_obj_counts [t] < 0))
            LOG_warn << "hwloc could not determine " << print (t) << " count";
    }

    // cache details:

    vr_static_assert (static_cast<int32_t> (cache_type::unified) == static_cast<int32_t> (HWLOC_OBJ_CACHE_UNIFIED));
    vr_static_assert (static_cast<int32_t> (cache_type::data) == static_cast<int32_t> (HWLOC_OBJ_CACHE_DATA));
    vr_static_assert (static_cast<int32_t> (cache_type::instr) == static_cast<int32_t> (HWLOC_OBJ_CACHE_INSTRUCTION));

    std::vector<state::cache_data_all_types> & cache_data = m_state->m_cache_data;

    for (int32_t lvl = /* ! */1; true; ++ lvl) // count L levels and populate 'cache_info' TODO limit by hwloc_topology_get_depth()
    {
        int32_t present_types { }; // sum of non-zero entries of 'present_count'
        int32_t present_count [cache_type::size] { 0 };
        int32_t hw_cache_depth [cache_type::size] { 0 }; // remember topology depths for further spec queries
        {
            for (cache_type::enum_t ct : cache_type::values ())
            {
                int32_t const hw_depth = ::hwloc_get_cache_type_depth (hw_topo, lvl, static_cast<::hwloc_obj_cache_type_e> (ct));
                if (hw_depth < 0) continue;
                hw_cache_depth [ct] = hw_depth;

                int32_t const count = ::hwloc_get_nbobjs_by_depth (hw_topo, hw_depth);
                if (! count) continue;


                present_count [ct] = count;
                ++ present_types;
            }
        }
        if (! present_types) // end of cache topology
            break;

        LOG_trace2 << "L" << lvl << ": " << present_count [0] << "|" << present_count [1] << "|" << present_count [2];

        // add the next L-entry:
        {
            cache_data.push_back ({ });
            state::cache_data_all_types & cdat = cache_data.back ();

            for (cache_type::enum_t ct : cache_type::values ())
            {
                cache::level & cd = cdat [ct];

                cd.m_count = present_count [ct];

                if (cd.m_count > 0) // further details:
                {
                    // assume that the topology tree is homogeneous within levels, get the first object
                    // as the exemplar:

                    ::hwloc_obj_t const first = ::hwloc_get_next_obj_by_depth (hw_topo, hw_cache_depth [ct], nullptr);
                    check_nonnull (first);
                    check_eq (first->type, HWLOC_OBJ_CACHE);

                    auto const & cache_attr = first->attr->cache;

                    cd.m_size = cache_attr.size;
                    cd.m_line_size = cache_attr.linesize;

                    if (cd.m_line_size != cache::static_line_size ())
                        LOG_warn << "build cache line assumption (" << cache::static_line_size () << ") != actual (" << cd.m_line_size << ')';
                }
            }
        }
    }
    cache_data.shrink_to_fit ();

    if (m_state->m_obj_counts [hw_obj::PU] > signed_cast (bit_set::bits_per_block)) // see convert() TODOs below
        LOG_warn << "count of PUs (" << m_state->m_obj_counts [hw_obj::PU] << ") too large, the affinity API needs upgrading";
}

cpu_info::~cpu_info () VR_NOEXCEPT
{
    ::hwloc_topology_t & hw_topo = m_state->m_hw_topo;

    if (hw_topo) ::hwloc_topology_destroy (hw_topo);
}
//............................................................................

int32_t
cpu_info::count (hw_obj::enum_t const obj) const
{
    return m_state->m_obj_counts [obj];
}
//............................................................................

cpu_info::cache const &
cpu_info::cache_info () const
{
    return m_state->m_cache;
}
//............................................................................
//............................................................................
namespace
{
// NOTE: not using either c++11 or Meyer singleton pattern here by design:
// all concurrent stores are atomic and will either read or store the same value
// so there is no need for locking or fencing

static cpu_info const * g_ci { nullptr }; // handle cached locally to this TU

VR_FORCEINLINE cpu_info const &
cpu_info_instance ()
{
    cpu_info const * r = mc::volatile_cast (g_ci);

    if (VR_UNLIKELY (r == nullptr))
        mc::volatile_cast (g_ci) = (r = & cpu_info::instance ());

    return (* r);
}
//............................................................................

using hwloc_bitmap_ptr      = std::unique_ptr<::hwloc_bitmap_s, void (*) (::hwloc_bitmap_t)>;

VR_FORCEINLINE hwloc_bitmap_ptr
make_hwloc_bitmap ()
{
    return { ::hwloc_bitmap_alloc (), ::hwloc_bitmap_free };
}
//............................................................................

// TODO: these are limited to 64 PUs max (which is checked at runtime) for now, but is easily extended
// to conversion in 64 bit blocks:

bit_set
convert (cpu_info const & ci, ::hwloc_bitmap_t const src)
{
    assert_nonnull (src);

    return bit_set { static_cast<bit_set::size_type> (ci.count (hw_obj::PU)), ::hwloc_bitmap_to_ulong (src) };
}

hwloc_bitmap_ptr
convert (cpu_info const & ci, bit_set const & src)
{
    hwloc_bitmap_ptr dst { make_hwloc_bitmap () };
    ::hwloc_bitmap_from_ulong (dst.get (), src.to_ulong ());

    return dst;
}

} // end of anonymous
//............................................................................
//............................................................................

bit_set
affinity::this_thread_PU_set ()
{
    cpu_info const & ci = cpu_info_instance ();
    ::hwloc_topology_t const hw_topo = ci.m_state->m_hw_topo;

    hwloc_bitmap_ptr const hw_PU_set { make_hwloc_bitmap () };

    VR_CHECKED_SYS_CALL (::hwloc_get_cpubind (hw_topo, hw_PU_set.get (), HWLOC_CPUBIND_THREAD ));

    return convert (ci, hw_PU_set.get ());
}

void
affinity::bind_this_thread (call_traits<bit_set>::param PU_set) VR_NOEXCEPT
{
    cpu_info const & ci = cpu_info_instance ();
    ::hwloc_topology_t const hw_topo = ci.m_state->m_hw_topo;

    hwloc_bitmap_ptr const hw_PU_set { convert (ci, PU_set) };
    VR_CHECKED_SYS_CALL_noexcept (::hwloc_set_cpubind (hw_topo, hw_PU_set.get (), (HWLOC_CPUBIND_THREAD | HWLOC_CPUBIND_STRICT)));
}

int32_t
affinity::this_thread_last_PU ()
{
    cpu_info const & ci = cpu_info_instance ();
    ::hwloc_topology_t const hw_topo = ci.m_state->m_hw_topo;

    hwloc_bitmap_ptr const hw_PU_set { make_hwloc_bitmap () };

    VR_CHECKED_SYS_CALL (::hwloc_get_last_cpu_location (hw_topo, hw_PU_set.get (), HWLOC_CPUBIND_THREAD));

    int32_t const r = ::hwloc_bitmap_first (hw_PU_set.get ());
    check_nonnegative (r);

    check_eq (::hwloc_bitmap_next (hw_PU_set.get (), r), -1); // a single bit is set

    return r;
}
//............................................................................

// TODO affinity syscalls are expensive, do them only if the PU sets requested are actually different from the current

struct affinity::scoped_thread_sched::pimpl final
{
    VR_FORCEINLINE pimpl (bit_set const & PU_set) :
        m_restore { this_thread_PU_set () }
    {
        bind_this_thread (PU_set);
    }

    VR_FORCEINLINE  ~pimpl ()
    {
        bind_this_thread (m_restore);
    }

    bit_set const m_restore;

}; // end of nested class
//............................................................................

affinity::scoped_thread_sched::scoped_thread_sched (bit_set const & PU_set) :
    m_impl { std::make_unique<pimpl> (PU_set) }
{
}

affinity::scoped_thread_sched::~scoped_thread_sched () VR_NOEXCEPT  = default; // pimpl

} // end of 'sys'
} // end of namespace
//----------------------------------------------------------------------------
