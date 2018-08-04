#pragma once

#include "vr/io/links/link_base.h"
#include "vr/io/net/socket_handle.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
//............................................................................
//............................................................................
namespace impl
{

class socket_link_base
{
    public: // ...............................................................

        socket_link_base (net::socket_handle && sh);
        ~socket_link_base () VR_NOEXCEPT; // calls 'close ()'


        void close () VR_NOEXCEPT; // idemptotent

    protected: // ............................................................

        static recv_arg_map select_recv_parms (recv_arg_map const & base, bool const requires_file, net::socket_handle const & sh, std::string const & link_name);
        static send_arg_map select_send_parms (send_arg_map const & base, bool const requires_file, net::socket_handle const & sh, std::string const & link_name);

        net::socket_handle m_socket;

}; // end of class
//............................................................................

template<bool ENABLED = false>
struct ancillary_context
{
}; // end of master

template<>
struct ancillary_context</* ENABLED */true>
{
    ancillary_context ()
    {
        m_iov.iov_len = 0; // *must* be reset by 'set_w_window()' before I/O commences

        std::memset (& m_mhdr, 0, sizeof (m_mhdr));
        {
            m_mhdr.msg_control = & m_cmhdr;

            m_mhdr.msg_iov = & m_iov;
            m_mhdr.msg_iovlen = 1;
        }
    }

    VR_FORCEINLINE struct ::msghdr * mhdr ()
    {
        return (& m_mhdr);
    }

    /*
     * note: this *must* be called before *each* I/O is attempted (including the first message)
     */
    VR_FORCEINLINE void reset_ancillary (addr_t const iov_addr, signed_size_t const w_window)
    {
        m_iov.iov_base = iov_addr;
        m_iov.iov_len = w_window; // note: if this does not change post-cosntruction, it can be set once only

        m_mhdr.msg_controllen = sizeof (m_cmhdr); // in/out parm
    }

    struct ::iovec m_iov;
    struct ::msghdr m_mhdr;
    union
    {
        struct ::cmsghdr m_cm;
        int8_t m_data [64];
    }
    m_cmhdr;

}; // end of specialization

} // end of 'impl'
//............................................................................
//............................................................................
/**
 */
template<typename DERIVED, typename ... ASPECTs>
class socket_link: public impl::socket_link_base, public link_base<DERIVED, ASPECTs ...>,
                   public impl::ancillary_context<(link_base<DERIVED, ASPECTs ...>::has_ts_last_recv ())> // EBO
{
    private: // ..............................................................

        using base          = impl::socket_link_base;
        using super         = link_base<DERIVED, ASPECTs ...>;

    protected: // ............................................................

        /*
         * currently, this is non-trivial only for _recv_
         */
        using ancillary     = impl::ancillary_context<(super::has_ts_last_recv ())>;

    public: // ...............................................................

        // TODO use link w_windows to size the socket send/recv buf sizes, not the other way around

        socket_link (recv_arg_map const & recv_args, send_arg_map const & send_args, net::socket_handle && sh, std::string const & name) :
            socket_link_base (std::move (sh)), // socket is constructed first
            super (select_recv_parms (recv_args, super::recv_requires_file (), m_socket, name), select_send_parms (send_args, super::send_requires_file (), m_socket, name))
        {
            if (super::has_state ()) super::state () = link_state::open;
        }

        socket_link (recv_arg_map const & recv_args, net::socket_handle && sh, std::string const & name) :
            socket_link_base (std::move (sh)), // socket is constructed first
            super (select_recv_parms (recv_args, super::recv_requires_file (), m_socket, name))
        {
            if (super::has_state ()) super::state () = link_state::open;
        }

        socket_link (send_arg_map const & send_args, net::socket_handle && sh, std::string const & name) :
            socket_link_base (std::move (sh)), // socket is constructed first
            super (select_send_parms (send_args, super::send_requires_file (), m_socket, name))
        {
            if (super::has_state ()) super::state () = link_state::open;
        }


        ~socket_link () VR_NOEXCEPT
        {
            close ();
        }

        void close () VR_NOEXCEPT // idemptotent
        {
            base::close (); // chain
        }

}; // end of class

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
