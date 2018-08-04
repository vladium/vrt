
#include "vr/stats/stream_stats.h"

#include <algorithm>

//----------------------------------------------------------------------------
namespace vr
{
namespace stats
{
//............................................................................
//............................................................................
namespace impl
{

std::vector<double> const &
check_ordered (std::vector<double> const & probabilities)
{
    check_nonempty (probabilities);

    // note: subtle bugs will occur if 'quantiles' is not sorted, so
    // guard against that:

    check_nonnegative (probabilities.front ());

    for (std::size_t i = /* ! */1; i < probabilities.size (); ++ i)
    {
        check_lt (probabilities [i - 1], probabilities [i]);
    }

    check_le (probabilities.back (), 1.0);

    return probabilities;
}

} // end of 'impl'
//............................................................................
//............................................................................

} // end of 'stats'
} // end of namespace
//----------------------------------------------------------------------------
