#pragma once

#include "vr/test/random.h"

#include "vr/data/defs.h"
#include "vr/data/NA.h"
#include "vr/util/datetime.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace data {
class dataframe; // forward
}

namespace test
{

template<data::atype::enum_t ATYPE, typename FORWARD_ITERATOR>
void
random_fill (FORWARD_ITERATOR first, FORWARD_ITERATOR last, double const NA_probability, int64_t & rnd, enum_int_t const labels_size = 0)
{
    if (ATYPE == data::atype::category) check_positive (labels_size);

    bool const allow_NAs = (NA_probability > 0);

    for ( ; first != last; ++ first)
    {
        switch (ATYPE) // compile-time switch
        {
            case data::atype::category:
            {
                if (allow_NAs && next_random<int64_t, double> (rnd) < NA_probability)
                    data::mark_NA (* first);
                else
                    * first = unsigned_cast (next_random (rnd)) % labels_size;
            }
            break;

            case data::atype::timestamp:
            {
                if (allow_NAs && next_random<int64_t, double> (rnd) < NA_probability)
                    data::mark_NA (* first);
                else
                    * first = unsigned_cast (next_random (rnd)) % util::timestamp_max ();
            }
            break;

            default:
            {
                using data_type     = typename data::atype::traits<ATYPE>::type;

                if (allow_NAs && next_random<int64_t, double> (rnd) < NA_probability)
                    data::mark_NA (* first);
                else
                    * first = next_random<int64_t, data_type> (rnd);
            }

        } // end of switch
    }
}

extern void
random_fill (data::dataframe & dst, int64_t & rnd, double const NA_probability = { });

} // end of 'test'
} // end of namespace
//----------------------------------------------------------------------------
