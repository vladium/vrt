#pragma once

#include "vr/data/NA.h"
#include "vr/market/defs.h"

#include <cmath>
#include <ratio>

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
//............................................................................

using price_si_t                        = util::scaled_int_t;  // "scaled int" price
using ratio_si_t                        = util::scaled_int_t;  // "scaled int" ratio/rate

constexpr price_si_t price_si_scale ()  { return util::si_scale (); }
constexpr ratio_si_t ratio_si_scale ()  { return util::si_scale (); }

//............................................................................
//............................................................................
namespace impl
{
/*
 *  fp price := book / price_si_scale () := wire / wire_scale
 */
template<typename T_LHS, typename T_RHS, typename/* lhs scale/rhs scale */SCALE_RATIO, bool NA_AWARE = true, typename = void>
struct scale_price; // master


template<typename T_LHS, typename T_RHS, typename SCALE_RATIO, bool NA_AWARE>
struct scale_price<T_LHS, T_RHS, SCALE_RATIO, NA_AWARE,
    util::enable_if_t<(std::is_integral<T_LHS>::value && std::is_integral<T_RHS>::value)>>
{
    using scale_ratio       = SCALE_RATIO;

    static T_LHS evaluate (T_RHS const & v)
    {
        if (NA_AWARE)
        {
            if (VR_UNLIKELY (data::is_NA (v))) // TODO optimize this check out when 'T_LHS' and 'T_RHS' are ints of the same size
                return data::NA<T_LHS> ();
        }

        return ((v * scale_ratio::num) / scale_ratio::den); // TODO guard against int mul overflow more carefully
    }

}; // end of specialization

// to 'double' (use only for human-friendly display):

template<typename T_LHS, typename T_RHS, typename SCALE_RATIO, bool NA_AWARE>
struct scale_price<T_LHS, T_RHS, SCALE_RATIO, NA_AWARE,
    util::enable_if_t<std::is_same<T_LHS, double>::value>>
{
    static constexpr double scale_ratio ()  { return (static_cast<double> (SCALE_RATIO::num) / SCALE_RATIO::den); }

    static double evaluate (T_RHS const & v)
    {
        if (NA_AWARE)
        {
            if (VR_UNLIKELY (data::is_NA (v)))
                return data::NA<double> ();
        }

        return (v * scale_ratio ());
    }

}; // end of specialization

// from 'double' (use only for human-friendly display):

template<typename T_LHS, typename T_RHS, typename SCALE_RATIO, bool NA_AWARE>
struct scale_price<T_LHS, T_RHS, SCALE_RATIO, NA_AWARE,
    util::enable_if_t<std::is_same<T_RHS, double>::value>>
{
    static constexpr double scale_ratio ()  { return (static_cast<double> (SCALE_RATIO::num) / SCALE_RATIO::den); }

    static T_LHS evaluate (double const & v)
    {
        if (NA_AWARE)
        {
            if (VR_UNLIKELY (data::is_NA (v)))
                return data::NA<T_LHS> ();
        }

        return std::round (v * scale_ratio ());
    }

}; // end of specialization

} // end of 'impl'
//............................................................................
//............................................................................

// TODO really need to use strong typedefs or tags here; there are three price domains:
//  1. wire
//  2. 'T_PRICE' as chosen by a book structure
//  3. print/human-friendly -> double or something fixed point-capable

/**
 * factored outside of 'price_ops' because doesn't depend on source-specific scaling
 */
template<typename T_BOOK, bool NA_AWARE = true>
double
price_book_to_print (T_BOOK const & v)
{
    return impl::scale_price<double,/* <= */T_BOOK, std::ratio<1, price_si_scale ()>, NA_AWARE>::evaluate (v);
}
/**
 * factored outside of 'price_ops' because doesn't depend on source-specific scaling
 */
template<typename T_BOOK, bool NA_AWARE = true>
T_BOOK
price_print_to_book (double const & v)
{
    return impl::scale_price<T_BOOK,/* <= */double, std::ratio<price_si_scale ()>, NA_AWARE>::evaluate (v);
}
//............................................................................

template<typename T_WIRE, typename/* book scale/wire scale */SCALE_RATIO, typename T_BOOK>
struct price_ops
{
    static T_BOOK wire_to_book (T_WIRE const & v)
    {
        return impl::scale_price<T_BOOK,/* <= */T_WIRE, SCALE_RATIO>::evaluate (v);

    }; // end of class
    /*
     * inverse of the above
     */
    static T_WIRE book_to_wire (T_BOOK const & v)
    {
        // 'NA_AWARE' set to 'false' since there's little point in putting NAs on the wire:

        return impl::scale_price<T_WIRE,/* <= */T_BOOK, std::ratio<SCALE_RATIO::den, SCALE_RATIO::num>, false>::evaluate (v);
    }


    static double wire_to_print (T_WIRE const & v)
    {
        return impl::scale_price<double,/* <= */T_WIRE, SCALE_RATIO>::evaluate (v);
    }
    /*
     * inverse of the above
     */
    static T_WIRE print_to_wire (double const & v)
    {
        return impl::scale_price<T_WIRE,/* <= */double, std::ratio_divide<std::ratio<price_si_scale ()>, SCALE_RATIO>>::evaluate (v);
    }

    static double book_to_print (T_BOOK const & v)
    {
        return price_book_to_print (v); // is NA-aware by default
    }
    /*
     * inverse of the above
     */
    static T_BOOK print_to_book (double const & v)
    {
        return price_print_to_book<T_BOOK> (v); // is NA-aware by default
    }

}; // end of class
//............................................................................

// TODO obsolete/rm these

template<typename R>
R
scale_for_output (price_si_t const price); // master


template<> // pass 'price_si_t' through
VR_FORCEINLINE price_si_t
scale_for_output<price_si_t> (price_si_t const price)
{
    return price;
}

template<> // convert to unscaled fp
VR_FORCEINLINE double
scale_for_output<double> (price_si_t const price)
{
    return (price / 10000000.0);
}
//............................................................................

} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
