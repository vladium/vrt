
#include "vr/util/function_traits.h"

#include <boost/function.hpp>

#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
//............................................................................

TEST (function_traits_test, lambda)
{
    auto l = [](int32_t const a0, int64_t const a1)  { return a0 * a1; };

    using traits        = function_traits<decltype (l)>;

    vr_static_assert (std::is_same<traits::result_type, int64_t>::value);
    vr_static_assert (traits::arity () == 2);

    vr_static_assert (std::is_same<traits::arg<0>::type, int32_t>::value);
    vr_static_assert (std::is_same<traits::arg<1>::type, int64_t>::value);
}

TEST (function_traits_test, std_function)
{
    std::function<int64_t (int32_t, int64_t)> f { [](int32_t const a0, int64_t const a1)  { return a0 * a1; } };

    using traits        = function_traits<decltype (f)>;

    vr_static_assert (std::is_same<traits::result_type, int64_t>::value);
    vr_static_assert (traits::arity () == 2);

    vr_static_assert (std::is_same<traits::arg<0>::type, int32_t>::value);
    vr_static_assert (std::is_same<traits::arg<1>::type, int64_t>::value);
}

TEST (function_traits_test, boost_function)
{
    boost::function<int64_t (int32_t, int64_t)> f { [](int32_t const a0, int64_t const a1)  { return a0 * a1; } };

    using traits        = function_traits<decltype (f)>;

    vr_static_assert (std::is_same<traits::result_type, int64_t>::value);
    vr_static_assert (traits::arity () == 2);

    vr_static_assert (std::is_same<traits::arg<0>::type, int32_t>::value);
    vr_static_assert (std::is_same<traits::arg<1>::type, int64_t>::value);
}


} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
