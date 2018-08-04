#pragma once

#include "vr/arg_map.h"
#include "vr/fields.h"
#include "vr/market/sources/asx/itch/ITCH_visitor.h"
#include "vr/meta/arrays.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
namespace ASX
{
/**
 */
template<typename CTX, typename DERIVED = void> // a slight twist on CRTP
class ITCH_ts_tracker: public ITCH_visitor<DERIVED>
{
    private: // ..............................................................

        vr_static_assert (has_field<_partition_, CTX> ()); // have to track origin clocks per partition

        using super         = ITCH_visitor<DERIVED>;
        using this_type     = ITCH_ts_tracker<CTX, DERIVED>;
        using derived       = util::if_is_void_t<DERIVED, this_type, DERIVED>; // a slight twist on CRTP

        static constexpr bool ctx_has_ts_origin ()      { return has_field<_ts_origin_, CTX> (); }

    public: // ...............................................................

        ITCH_ts_tracker ()  = default;

        ITCH_ts_tracker (arg_map const & args) :
            ITCH_ts_tracker { }
        {
        }


        // overridable visits:

        using super::visit;

        /*
         * ['seconds' is handled in a special way]
         */
        VR_ASSUME_HOT bool visit (itch::seconds const & msg, CTX & ctx) // override
        {
            if (ctx_has_ts_origin ())
            {
                auto const pix = field<_partition_> (ctx);

                timestamp_t const ts_origin_sec = msg.ts_sec () * _1_second ();

                /* TODO field<_ts_origin_> (ctx) = */m_ts_origin_sec [pix] = ts_origin_sec; // note: setting it in 'ctx' as well (coarse timestamp is better than nothing)
            }

            return super::visit (msg, ctx);
        }

#define vr_VISIT_MESSAGE(r, unused, MSG) \
        VR_FORCEINLINE bool visit (itch:: MSG const & msg, CTX & ctx) /* override */ \
        { \
            if (ctx_has_ts_origin ()) field<_ts_origin_> (ctx) = m_ts_origin_sec [field<_partition_> (ctx)] + msg.hdr ().ts_ns (); \
            \
            return super::visit (msg, ctx); \
        } \
        /* */


        // ['end_of_snapshot' is not timestamped]

        BOOST_PP_SEQ_FOR_EACH (vr_VISIT_MESSAGE, unused, VR_MARKET_ITCH_RECV_MESSAGE_TIMESTAMPED_SEQ)


#undef vr_VISIT_MESSAGE

    private: // ..............................................................

        std::array<timestamp_t, partition_count ()> m_ts_origin_sec { meta::create_array_fill<timestamp_t, partition_count (), 0> () };

}; // end of class

} // end of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
