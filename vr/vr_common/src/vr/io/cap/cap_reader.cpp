
#include "vr/io/cap/cap_reader.h"

#include "vr/io/pcap/pcaprec_hdr.h"

#include <pcap/pcap.h>

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
//............................................................................

cap_reader::cap_reader (std::istream & in, cap_format::enum_t const format) :
    m_in { in }
{
    switch (format)
    {
        case cap_format::pcap: ensure_buf_capacity (read_pcap_header (m_in)); break;

        default: break;

    } // end of switch
}
//............................................................................

void
cap_reader::ensure_buf_capacity (mapped_ring_buffer::size_type const capacity)
{
    if (VR_UNLIKELY (capacity > m_buf.capacity ()))
    {
        util::destruct (m_buf);
        new (& m_buf) mapped_ring_buffer { capacity };
    }
}
//............................................................................

int32_t
cap_reader::read_pcap_header (std::istream & in)
{
    ::pcap_file_header pcap_file_hdr;
    {
        auto const rc = read_fully (in, sizeof (pcap_file_hdr), & pcap_file_hdr);
        check_eq (rc, sizeof (pcap_file_hdr));
    }

    if (pcap_file_hdr.magic != VR_PCAP_MAGIC_LE_NS)
        throw_x (io_exception, "unexpected pcap magic number " + hex_string_cast (pcap_file_hdr.magic));

    check_eq (pcap_file_hdr.version_major, PCAP_VERSION_MAJOR);
    check_eq (pcap_file_hdr.version_minor, PCAP_VERSION_MINOR);

    check_zero (pcap_file_hdr.thiszone);
    check_condition ((pcap_file_hdr.linktype == DLT_EN10MB) | (pcap_file_hdr.linktype == DLT_LINUX_SLL), pcap_file_hdr.linktype); // ethernet or cooked

    LOG_trace1 << "opened pcap stream: linktype " << pcap_file_hdr.linktype << ", snaplen " << pcap_file_hdr.snaplen;

    return pcap_file_hdr.snaplen;
}

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
