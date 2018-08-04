
#include "vr/arg_map.h"
#include "vr/io/cap/cap_reader.h"
#include "vr/io/events/event_context.h"
#include "vr/io/files.h"
#include "vr/io/net/IP_.h"
#include "vr/io/net/pcap_.h"
#include "vr/io/net/UDP_.h"
#include "vr/io/stream_factory.h"
#include "vr/util/env.h"
#include "vr/util/ops_int.h"

#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
using namespace net;

//............................................................................
//............................................................................
namespace
{

struct OPRA_payload
{
    int64_t m_visit_count { };

    // packet visitor:

    OPRA_payload (arg_map const & args)
    {
    }


    template<typename CAP_HDR>
    VR_FORCEINLINE void visit_data (CAP_HDR const & cap_hdr, addr_const_t const data, int32_t const size)
    {
        check_positive (size);
        check_eq (byte_ptr_cast (data) [0], 5); // OPRA

        ++ m_visit_count;
    }

}; // end of class

struct empty_context    { };

} // end of anonymous
//............................................................................
//............................................................................

using contexts      = gt::Types
<
    empty_context,
    meta::make_compact_struct_t<meta::select_fields_t<event_schema>>,
    meta::make_compact_struct_t<meta::select_fields_t<event_schema, _packet_index_>>,
    meta::make_compact_struct_t<meta::select_fields_t<event_schema, _dst_port_>>,
    meta::make_compact_struct_t<meta::select_fields_t<event_schema,                 _ts_local_>>,
    meta::make_compact_struct_t<meta::select_fields_t<event_schema, _packet_index_, _ts_local_>>,
    meta::make_compact_struct_t<meta::select_fields_t<event_schema,                 _ts_local_, _ts_local_delta_>>,
    meta::make_compact_struct_t<meta::select_fields_t<event_schema, _packet_index_, _ts_local_, _ts_local_delta_>>
>;

template<typename T> struct cap_reader_test: gt::Test {};
TYPED_TEST_CASE (cap_reader_test, contexts);

//............................................................................

TYPED_TEST (cap_reader_test, OPRA_sample) // c.f. 'pcap_reader_test.OPRA_sample'
{
    fs::path const in_file { test::data_dir () / "test.49999.pcap.gz" };

    using ctx               = TypeParam; // [testcase parameter]
    using reader            = cap_reader;                       // generic capture reader
    using visitor           = pcap_<IP_<UDP_<OPRA_payload>>>;   // pcap encapsulation added to the visitor

    int64_t const packet_count_expected = 49999;
    int64_t r_packet_count { };

    visitor v { { } };
    {
        std::unique_ptr<std::istream> const in = stream_factory::open_input (in_file);

        ctx c { };
        reader r { * in, cap_format::pcap };                    // 'pcap' format passed to the constructor at runtime

        r_packet_count = r.evaluate (c, v);
    }
    EXPECT_EQ (r_packet_count, packet_count_expected);
    EXPECT_EQ (v.m_visit_count, packet_count_expected); // reader and visitor better agree
}

TYPED_TEST (cap_reader_test, IGMP_sample)
{
    fs::path const in_file { test::data_dir () / "test.igmp_pim.176.pcap.gz" }; // non-UDP/TCP traffic, some packet need eth padding

    using ctx               = TypeParam; // [testcase parameter]
    using reader            = cap_reader;                       // generic capture reader
    using visitor           = pcap_<IP_<UDP_<>>>;               // pcap encapsulation added to the [null] visitor

    int64_t const packet_count_expected = 176;
    int64_t r_packet_count { };

    visitor v { { } };
    {
        std::unique_ptr<std::istream> const in = stream_factory::open_input (in_file);

        ctx c { };
        reader r { * in, cap_format::pcap };                    // 'pcap' format passed to the constructor at runtime

        r_packet_count = r.evaluate (c, v);
    }
    EXPECT_EQ (r_packet_count, packet_count_expected);
}


} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
