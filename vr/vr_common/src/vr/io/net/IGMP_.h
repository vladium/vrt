#pragma once

#include "vr/arg_map.h"
#include "vr/fields.h"
#include "vr/io/net/defs.h" // print() for net addrs
#include "vr/util/datetime.h" // print_timestamp
#include "vr/util/logging.h"

#include <netinet/igmp.h>

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
namespace net
{

class IGMP_
{
    public: // ...............................................................

        static constexpr int32_t proto ()       { return IPPROTO_IGMP; }

        static constexpr int32_t read_offset () { return sizeof (::igmp); }
        static constexpr int32_t min_size ()    { return read_offset (); }


        IGMP_ (arg_map const & args)
        {
        }

        /**
         * 'size' bytes of data is available starting at 'udp_hdr'
         *
         * populates 'CTX' fields, if defined:
         *
         *   net::_filtered_
         */
        template<typename CTX>
        VR_FORCEINLINE void
        visit_proto (CTX & ctx, ::igmp const * const igmp_hdr, int32_t const size)
        {
            assert_le (read_offset (), size);

            VR_IF_DEBUG
            (
                using _ts_          = util::if_t<(has_field<_ts_local_, CTX> ()), _ts_local_, _ts_origin_>;

                ++ m_IGMP_packet_count;
                DLOG_trace2 << "\t[" << m_IGMP_packet_count << ", " << print_timestamp (field<_ts_> (ctx)) << "] size: " << size << ", IGMP request: " << hex_string_cast (static_cast<uint32_t> (igmp_hdr->igmp_type)) << ", group: " << print (igmp_hdr->igmp_group);
            )

            if (has_field<_filtered_, CTX> ())
            {
                field<_filtered_> (ctx) = true;
            }
        }

    private: // ..............................................................

        VR_IF_DEBUG (int64_t m_IGMP_packet_count { };)

}; // end of class

} // end of 'net'
} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
