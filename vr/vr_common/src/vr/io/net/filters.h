#pragma once

#include "vr/fields.h"
#include "vr/io/net/defs.h" // _packet_index_
#include "vr/util/logging.h"
#include "vr/util/ops_int.h"
#include "vr/util/type_traits.h"

#include <netinet/ip.h>

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
//............................................................................
//............................................................................
namespace impl
{

template<typename HANDLER, typename HANDLER_FALLBACK>
struct dispatch
{
    template<typename T, typename CTX, typename HDR>
    static VR_FORCEINLINE void
    evaluate (T * const obj, CTX & ctx, HDR const * const data, int32_t const size)
    {
        static_cast<HANDLER *> (obj)->visit_proto (ctx, static_cast<HDR const *> (data), size);
    }
};

template<typename HANDLER_FALLBACK>
struct dispatch</* HANDLER */void, HANDLER_FALLBACK>
{
    template<typename T, typename CTX, typename HDR>
    static VR_FORCEINLINE void
    evaluate (T * const obj, CTX & ctx, HDR const * const data, int32_t const size)
    {
        static_cast<HANDLER_FALLBACK *> (obj)->visit_proto (ctx, static_cast<HDR const *> (data), size);
    }
};

} // end of 'impl'
//............................................................................
//............................................................................

template<typename T, typename = void>
struct proto_of
{
    static constexpr int32_t value      = -1; // TODO safer value

}; // end of master

template<typename T>
struct proto_of<T,
    util::void_t<decltype (T::proto ()) > >
{
    static constexpr int32_t value      = T::proto ();

}; // end of specialization
//............................................................................
/*
 * selects first (T::proto() == V) match in 'Ts' (or 'void' if no match)
 */
template<int32_t V, typename ... Ts>
struct select_by_proto; // forward


template<int32_t V>
struct select_by_proto<V>
{
    using type          = void;

}; // end of specialization

template<int32_t V, typename T, typename ... Ts>
struct select_by_proto<V, T, Ts ...>
{
    using type          = util::if_t<(proto_of<T>::value == V), T, typename select_by_proto<V, Ts ...>::type>;

}; // end of specialization
//............................................................................

class drop_packet_handler
{
    public: // ...............................................................

        static constexpr int32_t min_size ()    { return 0; } // conservative assumption: don't know all the protos we might encounter


        template<typename CTX, typename HDR>
        VR_FORCEINLINE void
        visit_proto (CTX & ctx, HDR const * const data, int32_t const size)
        {
            if (has_field<net::_filtered_, CTX> ())
            {
                field<net::_filtered_> (ctx) = true;
            }

            constexpr bool have_packet_index    = has_field<net::_packet_index_, CTX> ();

            if (have_packet_index) // compile-time branch
                LOG_trace1 << "filtering {" << util::class_name<HDR> () << "} packet #" << static_cast<int64_t> (field<net::_packet_index_> (ctx));
            else
                LOG_trace1 << "filtering {" << util::class_name<HDR> () << "} packet";
        }

        template<typename CTX>
        VR_FORCEINLINE void
        mark_filtered (CTX & ctx, ::ip const * const ip_hdr, int32_t const size)
        {
            if (has_field<net::_filtered_, CTX> ())
            {
                field<net::_filtered_> (ctx) = true;
            }

            // in debug builds, log a warning *once* per unique 'ip_p' value:

            VR_IF_DEBUG
            (
                int32_t const ip_p_bit = (1 << ip_hdr->ip_p);

                if (VR_LIKELY (m_ip_p_warned & ip_p_bit))
                    return;

                m_ip_p_warned |= ip_p_bit;

                constexpr bool have_packet_index    = has_field<net::_packet_index_, CTX> ();

                if (have_packet_index) // compile-time branch
                    DLOG_warn << "[packet #" << static_cast<int64_t> (field<net::_packet_index_> (ctx)) << "]: ignoring IP protocol " << static_cast<uint32_t> (ip_hdr->ip_p);
                else
                    DLOG_warn << "ignoring IP protocol " << static_cast<uint32_t> (ip_hdr->ip_p);
            )
        }

    private: // ..............................................................

        VR_IF_DEBUG (bitset32_t m_ip_p_warned { };) // note: actually only need a byte

}; // end of class
//............................................................................

struct IP_range_filter final
{
    IP_range_filter (uint32_t const lo, uint32_t const hi) :
        m_lo_h { lo },
        m_hi_m_lo_h { hi - m_lo_h }
    {
        check_le (lo, hi);
    }

    VR_FORCEINLINE bool operator() (::in_addr_t const addr) const
    {
        return (unsigned_cast (util::net_to_host (addr) - m_lo_h) <= m_hi_m_lo_h);
    }

    uint32_t const m_lo_h;
    uint32_t const m_hi_m_lo_h;

}; // end of class

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
