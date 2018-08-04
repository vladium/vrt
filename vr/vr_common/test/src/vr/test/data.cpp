
#include "vr/test/data.h"

#include "vr/data/dataframe.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace test
{
using namespace data;

//............................................................................
//............................................................................
namespace
{

struct random_fill_visitor final // stateless
{
    VR_FORCEINLINE void operator() (atype_<atype::category>, dataframe & df, attribute const & a, width_type const col, double const NA_probability, int64_t & rnd) const
    {
        enum_int_t * VR_RESTRICT const col_data = df.at<enum_int_t> (col);

        random_fill<atype::category> (col_data, col_data + df.row_count (), NA_probability, rnd, a.labels ().size ());
    }

    VR_FORCEINLINE void operator() (atype_<atype::timestamp>, dataframe & df, attribute const & a, width_type const col, double const NA_probability, int64_t & rnd) const
    {
        timestamp_t * VR_RESTRICT const col_data = df.at<timestamp_t> (col);

        random_fill<atype::timestamp> (col_data, col_data + df.row_count (), NA_probability, rnd);
    }

    template<atype::enum_t ATYPE>
    VR_FORCEINLINE void operator() (atype_<ATYPE>, dataframe & df, attribute const & a, width_type const col, double const NA_probability, int64_t & rnd) const
    {
        using data_type             = typename atype::traits<ATYPE>::type;

        data_type * VR_RESTRICT const col_data = df.at<data_type> (col);

        random_fill<ATYPE> (col_data, col_data + df.row_count (), NA_probability, rnd);
    }

}; // end of class

} // end of anonymous
//............................................................................
//............................................................................

void
random_fill (dataframe & dst, int64_t & rnd, double const NA_probability)
{
    check_positive (dst.row_count ()); // catch attempts to fill an unresized df

    attr_schema const & as = * dst.schema ();
    width_type const col_count { as.size () };

    for (width_type c = 0; c < col_count; ++ c)
    {
        attribute const & a = as [c];

        dispatch_on_atype (a.atype (), random_fill_visitor { }, dst, a, c, NA_probability, rnd);
    }
}

} // end of 'test'
} // end of namespace
//----------------------------------------------------------------------------
