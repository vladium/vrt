
#include "vr/io/links/socket_link.h"

#include <algorithm>

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
//............................................................................
//............................................................................

inline fs::path // internal linkage
set_file (std::string const & link_name, bool const is_recv, arg_map & out)
{
    if (out.count ("file")) // if set, overrides everything else
        return out.get<fs::path> ("file");

    fs::path path = out.get<fs::path> ("path");

    path += join_as_name ((is_recv ? ".recv" : ".send"), link_name);

    out ["file"] = path;

    return path;
}
//............................................................................
//............................................................................
namespace impl
{

socket_link_base::socket_link_base (net::socket_handle && sh) :
    m_socket { std::move (sh) }
{
}

socket_link_base::~socket_link_base () VR_NOEXCEPT
{
    close ();
}
//............................................................................

void
socket_link_base::close () VR_NOEXCEPT
{
    m_socket.close (); // idempotent
}
//............................................................................

recv_arg_map
socket_link_base::select_recv_parms (recv_arg_map const & base, bool const requires_file, net::socket_handle const & sh, std::string const & link_name)
{
    recv_arg_map r (base);

    window_t w_window = r.get<window_t> ("w_window", 0);
    w_window = std::max<window_t> (w_window, sh.read_buf_size ()); // ensure a full socket recv buf can be accommodated
    r ["w_window"] = w_window;

    capacity_t capacity = r.get<capacity_t> ("capacity", 0); // ideally, it is >> 'w_window'
    capacity = std::max<capacity_t> (capacity, 2 * w_window); // ensure 'capacity >= w_window' constraint some buffer types have
    r ["capacity"] = capacity;

    LOG_trace1 << "selected recv capacity: " << capacity << ", w_window: " << w_window;

    if (requires_file)
    {
        fs::path const file = set_file (link_name, true, r);
        LOG_trace1 << "buffer file: " << print (file);
    }

    return r;
}

send_arg_map
socket_link_base::select_send_parms (send_arg_map const & base, bool const requires_file, net::socket_handle const & sh, std::string const & link_name)
{
    send_arg_map r (base);

    // ensure 'capacity >= w_window' constraint some buffer types have

    // TODO unclear how to account for 'sh' send buf size

    window_t w_window = r.get<window_t> ("w_window", 0);
    capacity_t capacity = r.get<capacity_t> ("capacity"); // ideally, it is >> 'w_window'

    if (w_window <= 0) // was not provided
        w_window = std::max<window_t> (2 * 1024, capacity / 2);
    else // was provided
        capacity = std::max<capacity_t> (capacity, 2 * w_window);

    r ["w_window"] = w_window;
    r ["capacity"] = capacity;

    LOG_trace1 << "selected send capacity: " << capacity << ", w_window: " << w_window;

    if (requires_file)
    {
        fs::path const file = set_file (link_name, false, r);
        LOG_trace1 << "buffer file: " << print (file);
    }

    return r;
}

} // end of 'impl'
//............................................................................
//............................................................................
} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
