
#include "vr/io/pcap/pcap_reader.h"

#include "vr/util/classes.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
//............................................................................
//............................................................................
namespace impl
{
//............................................................................

pcap_reader_base::pcap_reader_base (std::istream & in) :
    m_in { in }
{
    ensure_buf_capacity (read_header (m_in));
}
//............................................................................

void
pcap_reader_base::ensure_buf_capacity (mapped_ring_buffer::size_type const capacity)
{
    if (VR_UNLIKELY (capacity > m_buf.capacity ()))
    {
        util::destruct (m_buf);
        new (& m_buf) mapped_ring_buffer { capacity };
    }
}
//............................................................................

int32_t
pcap_reader_base::read_header (std::istream & in)
{
    ::pcap_file_header file_hdr;
    {
        auto const rc = read_fully (in, sizeof (file_hdr), & file_hdr);
        check_eq (rc, sizeof (file_hdr));
    }

    if (file_hdr.magic != VR_PCAP_MAGIC_LE_NS)
        throw_x (io_exception, "unexpected pcap magic number " + hex_string_cast (file_hdr.magic));

    check_eq (file_hdr.version_major, PCAP_VERSION_MAJOR);
    check_eq (file_hdr.version_minor, PCAP_VERSION_MINOR);

    check_zero (file_hdr.thiszone);
    check_condition ((file_hdr.linktype == DLT_EN10MB) | (file_hdr.linktype == DLT_LINUX_SLL), file_hdr.linktype); // ethernet or cooked

    LOG_trace1 << "opened pcap stream: linktype " << file_hdr.linktype << ", snaplen " << file_hdr.snaplen;

    return file_hdr.snaplen;
}

} // end of 'impl'
//............................................................................
//............................................................................
} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
