#pragma once

#include "vr/util/type_traits.h"

#include <functional>
#include <tuple>

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
/*
 * see https://stackoverflow.com/a/7943765 for this nifty trick
 */
template<typename F>
struct function_traits: public function_traits<decltype (& F::operator ())>
{
}; // end of metafunction

template<typename T, typename R, typename ... ARGs>
struct function_traits<R (T:: *)(ARGs ...) const>
{
    using result_type       = R;

    static constexpr std::size_t arity ()   { return sizeof ... (ARGs); }

    template<size_t INDEX>
    struct arg
    {
        using type          = typename std::tuple_element<INDEX, std::tuple<ARGs ...> >::type;
    };

}; // end of specialization

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
