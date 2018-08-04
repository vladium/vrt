#pragma once

#include "vr/asserts.h"
#include "vr/util/function_traits.h"
#include "vr/sys/defs.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace sys
{
class affinity; // forward

/**
 * singleton API for querying system data like PU and core counts, as well as
 * details of various cache levels (total sizes, cache line sizes, etc)
 */
class cpu_info final: noncopyable
{
    private: // ..............................................................

        class state; // forward

    public: // ...............................................................

        struct cache final: noncopyable
        {
            static constexpr int32_t static_line_size () { return 64; } // TODO make a build parameter

            struct level final
            {
                std::size_t m_size { };     // cache size
                int32_t m_line_size { };    // cache line size
                int32_t m_count { };        // total number of caches at this level (and of this type) in the system

            }; // end of nested class

            /**
             * @return count of distinct levels of cache (e.g. 3 for a system with L1, L2, L3)
             */
            int32_t level_count () const;
            /**
             * equivalent to 'level_count()'
             */
            int32_t LLC () const
            {
                return level_count ();
            }

            /**
             *
             * @param lvl [1-based indexing, see also @ref sys::L1]
             * @param ct
             */
            level const & level_info (int32_t const lvl, cache_type::enum_t const ct = cache_type::data) const;


            private: addr_const_t m_impl; friend class state;

        }; // end of nested class


        static cpu_info const & instance ();
        VR_ASSUME_COLD ~cpu_info () VR_NOEXCEPT;


        /**
         * @return [-1 if could not be determined]
         */
        int32_t count (hw_obj::enum_t const obj) const;
        /**
         * shortcut for 'count (hw_obj::PU)'
         */
        int32_t PU_count () const
        {
            return count (hw_obj::PU);
        }

        /**
         * @return object with detailed information on the system's caches
         */
        cache const & cache_info () const;


        template<typename A> class access_by;

    private: // ..............................................................

        friend class affinity;
        template<typename A> friend class access_by;

        VR_ASSUME_COLD cpu_info ();

        std::unique_ptr<state> const m_state; // pimpl

}; // end of class
//............................................................................
/**
 * a static API for managing process/thread affinities
 */
class affinity final: noncopyable // not using a namespace for ease of friending
{
    public: // ...............................................................

        static bit_set this_thread_PU_set ();

        static void bind_this_thread (call_traits<bit_set>::param PU_set) VR_NOEXCEPT;

        /**
         * @return PU on which this thread ran last [note that there is a race here unless the thread was pinned
         *         to a singleton PU set previously]
         *
         * @see sys::cpuid ()
         */
        static int32_t this_thread_last_PU ();

        /**
         * a scoped RAII thread affinity changer/restorer
         */
        class scoped_thread_sched final: noncopyable
        {
            public:

                VR_ASSUME_HOT scoped_thread_sched (bit_set const & PU_set);
                VR_ASSUME_HOT ~scoped_thread_sched () VR_NOEXCEPT;

            private:

                class pimpl; // forward

                std::unique_ptr<pimpl> const m_impl;

        }; // end of nested class

        /**
         * execute 'f' on a given 'PU' (useful for optimizing NUMA, etc memory allocation)
         * @param PU
         */
        template<typename F, typename ... ARGs>
        static auto run_on_PU (int32_t const PU, F && f, ARGs && ... args) -> typename util::function_traits<F>::result_type
        {
            auto const PU_count = cpu_info::instance ().PU_count ();
            check_within (PU, PU_count);

            scoped_thread_sched _ { make_bit_set (PU_count).set (PU) };
            {
                return f (std::forward<ARGs> (args) ...);
            }
        }

}; // end of class

} // end of 'sys'
} // end of namespace
//----------------------------------------------------------------------------
