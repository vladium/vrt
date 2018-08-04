#pragma once

#include "vr/market/sources/asx/defs.h" // order_token

#include "vr/util/ops_int.h"
#include "vr/util/type_traits.h"

#include "vr/util/logging.h" // TODO rm

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
namespace ASX
{

class order_token_generator: noncopyable
{
    private: // ..............................................................

        using int_ops       = util::ops_int<util::arg_policy<util::zero_arg_policy::ignore, 0>, false>; // unchecked

        using storage_type  = util::unsigned_type_of_size<order_token::max_size ()>::type;

        static constexpr native_uint128_t digits_hi ()  { return 0x4645444342413938; }
        static constexpr native_uint128_t digits_lo ()  { return 0x3736353433323130; }

        static constexpr native_uint128_t digits ()     { return ((digits_hi () << 64) | digits_lo ()); }

    public: // ...............................................................

        static order_token make (uint32_t const v)
        {
            return order_token { convert (v) };
        }

        template<int32_t PREFIX_DIGITS, bool _ = ((PREFIX_DIGITS > 0) & (PREFIX_DIGITS < 8))>
        static auto make_with_prefix (uint32_t const v, uint32_t const prefix)
            -> util::enable_if_t<_, order_token>
        {
            return order_token { convert_fw (v | (prefix << ((sizeof (uint64_t) - PREFIX_DIGITS) * 4))) };
        }

        template<int32_t PREFIX_DIGITS, bool _ = ((PREFIX_DIGITS > 0) & (PREFIX_DIGITS < 8))>
        static auto make_with_prefix (uint32_t const v, uint32_t const prefix)
            -> util::enable_if_t<(! _), order_token>
        {
            return order_token { convert_fw (v) };
        }

    private: // ..............................................................

        template<typename I>
        static VR_FORCEINLINE storage_type convert (I v)
        {
            int32_t const hdc_floor = (int_ops::log2_floor (v) >> 2);
            assert_lt (hdc_floor, order_token::max_size ());

            storage_type data { };

            int32_t shift = (hdc_floor << 3);
            for (int32_t i = 0; i <= hdc_floor; ++ i)
            {
                data |= ((static_cast<native_uint128_t> (static_cast<uint32_t> (digits () >> ((v & 0x0F) << 3)) & 0xFF)) << shift);

                v >>= 4;
                shift -= 8;
            }

            return data;
        }


        template<typename I> // TODO this is hardcoded for a 32-bit 'I'
        static VR_FORCEINLINE storage_type convert_fw (I const v)
        {
#       define vr_DIGIT(z, DIX, d) \
            storage_type const data_##DIX = ((static_cast<native_uint128_t> (static_cast<uint32_t> (digits () >> (((v >> ( DIX * 4 )) & 0x0F) << 3)) & 0xFF)) << (56 - DIX * 8)); \
            /* */

            BOOST_PP_REPEAT_FROM_TO (0, 8, vr_DIGIT, unused)

#       undef vr_DIGIT

#       define vr_OR(z, DIX, d)     | data_##DIX

            storage_type const r = 0 BOOST_PP_REPEAT_FROM_TO (0, 8, vr_OR, unused);

#       undef vr_OR

            return r;
        }

}; // end of class

} // end of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
