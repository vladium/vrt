
#include "vr/containers/util/stable_vector.h"
#include "vr/util/memory.h"

#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
//............................................................................

TEST (stable_vector, stability)
{
    using value_type        = int64_t;

    stable_vector<value_type> sv; // note: not reserving mem at construction time

    int32_t const i_limit   = 10000;
    std::unique_ptr<value_type const * []> addrs { std::make_unique<value_type const * []> (i_limit) };

    for (int32_t i = 0; i < i_limit; ++ i)
    {
        sv.push_back (i);
        addrs [i] = & sv.back ();
    }

    for (int32_t i = 0; i < i_limit; ++ i)
    {
        ASSERT_EQ (& sv [i], addrs [i]) << "address changed for i = " << i;
    }
}

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
