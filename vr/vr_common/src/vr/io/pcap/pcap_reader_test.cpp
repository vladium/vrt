
#include "vr/arg_map.h"
#include "vr/io/files.h"
#include "vr/io/net/IP_.h"
#include "vr/io/net/UDP_.h"
#include "vr/io/pcap/pcap_reader.h"
#include "vr/io/stream_factory.h"
#include "vr/util/env.h"
#include "vr/util/ops_int.h"

#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
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

} // end of anonymous
//............................................................................
//............................................................................

using header_variants   = gt::Types
<
    pcaprec_hdr<util::subsecond_duration_us, false>,
    pcaprec_hdr<util::subsecond_duration_us, true>,

    pcaprec_hdr<util::subsecond_duration_ns, false>,
    pcaprec_hdr<util::subsecond_duration_ns, true>
>;

template<typename T> struct pcaprec_hdr_test: public gt::Test { };
TYPED_TEST_CASE (pcaprec_hdr_test, header_variants);

TYPED_TEST (pcaprec_hdr_test, accessors)
{
    using header_type       = TypeParam; // test parameter

    header_type h;

    vr_static_assert (meta::has_field<_ts_local_, header_type> ()); // synthetic field


    constexpr int32_t round = (header_type::ts_fraction_limit () == _1_second () ? 1 : 1000);

    for (timestamp_t ts : { 0L, 1L, 12345678L, 1000009 * _1_second () + 3 * _1_microsecond () + 9 * _1_nanosecond () })
    {
        timestamp_t const ts_rounded = (ts / round) * round; // value that can be stored in 'h' without loss

        field<_ts_local_> (h) = ts_rounded;
        timestamp_t const ts_read_back = field<_ts_local_> (h);

        ASSERT_EQ (ts_read_back, ts_rounded);
    }
}
//............................................................................

#define vr_TEST_PCAP            "test.49999.pcap.gz"
#define vr_TEST_PACKET_COUNT    (49999)

TEST (pcap_reader_test, OPRA_sample) // c.f. 'cap_reader_test.OPRA_sample'
{
    using namespace net;

    fs::path const in_file { test::data_dir () / vr_TEST_PCAP };

    using reader            = pcap_reader<>;            // pcap-specific reader
    using visitor           = IP_<UDP_<OPRA_payload>>;  // no pcap encapsulation added to the visitor

    int64_t const packet_count_expected = vr_TEST_PACKET_COUNT;

    int64_t r_packet_count { };
    visitor v { { } };
    {
        std::unique_ptr<std::istream> const in = stream_factory::open_input (in_file);

        reader r { * in };

        r_packet_count = r.evaluate (v);
    }
    EXPECT_EQ (r_packet_count, packet_count_expected);
    EXPECT_EQ (v.m_visit_count, packet_count_expected); // reader and visitor better agree
}

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
