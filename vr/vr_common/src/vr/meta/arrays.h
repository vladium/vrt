#pragma once

#include <array>

//----------------------------------------------------------------------------
namespace vr
{
namespace meta
{
//............................................................................
//............................................................................
namespace impl
{

template<typename T, T ... Vs>
struct value_sequence { };

//............................................................................

template<typename T, T ... Vs>
constexpr std::array<T, sizeof ... (Vs)>
create_array_impl (value_sequence<T, Vs ...>)
{
    return std::array<T, sizeof ... (Vs)> { { Vs ... } };
}
//............................................................................

template<typename T, T V, typename V_SEQ>
struct append_value;


template<typename T, T V, T ... Vs> // recursion step
struct append_value<T, V, value_sequence<T, Vs ...> >
{
    using type              = value_sequence<T, Vs ..., V>;

}; // end of specialization
//............................................................................

template<typename T, std::size_t N, T V, std::size_t I>
struct repeat_value;


template<typename T, std::size_t N, T V> // end of recursion
struct repeat_value<T, N, V, N>
{
    using type              = value_sequence<T>;

}; // end of specialization

template<typename T, std::size_t N, T V, std::size_t I> // recursion step
struct repeat_value
{
    using type              = typename append_value<T, V, typename repeat_value<T, N, V, (I + 1)>::type>::type;

}; // end of metafunction

} // end of 'impl'
//............................................................................
//............................................................................

template<typename V_SEQ>
constexpr auto
create_array (V_SEQ && vs) -> decltype (impl::create_array_impl (vs))
{
    return impl::create_array_impl (vs);
}
//............................................................................

template<typename T, std::size_t N, T V>
constexpr std::array<T, N>
create_array_fill ()
{
    using val_seq       = typename impl::repeat_value<T, N, V, 0>::type;

    return create_array (val_seq { });
}

} // end of 'meta'
} // end of namespace
//----------------------------------------------------------------------------
