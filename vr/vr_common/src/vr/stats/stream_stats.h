#pragma once

#include "vr/asserts.h"

VR_DIAGNOSTIC_PUSH ()
VR_DIAGNOSTIC_IGNORE ("-Wdeprecated-declarations")
#   include <boost/accumulators/accumulators.hpp>
#   include <boost/accumulators/statistics/count.hpp>
#   include <boost/accumulators/statistics/extended_p_square.hpp>
VR_DIAGNOSTIC_POP ()

//----------------------------------------------------------------------------
namespace vr
{
namespace stats
{
//............................................................................
//............................................................................
namespace impl
{
namespace ba    = boost::accumulators;


extern std::vector<double> const &
check_ordered (std::vector<double> const & probabilities); // check ordering, duplicates, etc


template<typename VARIATE>
class stream_stats_impl final: noncopyable
{
    public: // ...............................................................

        stream_stats_impl (std::vector<double> const & probabilities) :
            m_acc_set { ba::tag::extended_p_square::probabilities = check_ordered (probabilities) },
            m_size { static_cast<int32_t> (probabilities.size ()) }
        {
        }

        // ACCESSORs:

        /**
         * @return sample count so far
         */
        std::size_t count () const
        {
            return ba::count (m_acc_set);
        }

        /**
         * @return quantile estimate for probability at given 'index'
         */
        double operator[] (const int32_t index) const
        {
            check_within (index, m_size);

            return ba::extended_p_square (m_acc_set)[index];
        }

        /**
         * @return number of probabilities tracked (as passed into the constructor)
         */
        int32_t const & size () const
        {
            return m_size;
        }

        // MUTATORs:

        VR_FORCEINLINE void operator() (VARIATE const & sample)
        {
            m_acc_set (sample);
        }

    private: // ..............................................................

        using acc_set       = ba::accumulator_set<VARIATE, ba::features
                                <
                                    ba::tag::count,
                                    ba::tag::extended_p_square // note: depends on 'count' anyway
                                > >;

        acc_set m_acc_set;
        int32_t const m_size;

}; // end of class

} // end of 'impl'
//............................................................................
//............................................................................
/**
 * a limited-memory stream quantile estimator (based on P^2 algorithm)
 */
template<typename VARIATE>
using stream_stats          = impl::stream_stats_impl<VARIATE>;

} // end of 'stats'
} // end of namespace
//----------------------------------------------------------------------------
