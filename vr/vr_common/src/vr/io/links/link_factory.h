#pragma once

#include "vr/arg_map.h"
#include "vr/filesystem.h"
#include "vr/io/links/link_base.h"
#include "vr/io/net/mcast_source.h"
#include "vr/io/net/socket_handle.h"
#include "vr/util/datetime.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
//............................................................................

// environment variables:

#if !defined (VR_ENV_LINK_CAP_ROOT)
#   define VR_ENV_LINK_CAP_ROOT     VR_APP_NAME "_LINK_CAP_ROOT"
#endif

//............................................................................
/**
 * @note this also performs the necessary mkdirs
 */
extern fs::path
create_link_capture_path (std::string const & name_prefix, fs::path const & root, util::ptime_t const & stamp);

/**
 * @note this also performs the necessary mkdirs
 */
extern fs::path
create_link_capture_path (std::string const & name_prefix, arg_map const & opt_args = { });

//............................................................................

template<typename BUFFER_TAG>
struct set_buffer_args; // master

// TODO forward 'name_prefix':

template<>
struct set_buffer_args<_ring_>
{
    static void evaluate (recv_arg_map & out, std::string const & name_prefix, arg_map const & opt_args) // "capacity" set by the caller
    {
        out.insert (opt_args.begin (), opt_args.end ());
    }

    static void evaluate (send_arg_map & out, std::string const & name_prefix, arg_map const & opt_args) // "capacity" set by the caller
    {
        out.insert (opt_args.begin (), opt_args.end ());
    }

}; // end of specialization

template<>
struct set_buffer_args<_tape_>
{
    static void evaluate (recv_arg_map & out, std::string const & name_prefix, arg_map const & opt_args) // "capacity" set by the caller
    {
        out.insert (opt_args.begin (), opt_args.end ());
        if (! out.count ("path")) out.emplace ("path", create_link_capture_path (name_prefix, opt_args));
    }

    static void evaluate (send_arg_map & out, std::string const & name_prefix, arg_map const & opt_args) // "capacity" set by the caller
    {
        out.insert (opt_args.begin (), opt_args.end ());
        if (! out.count ("path")) out.emplace ("path", create_link_capture_path (name_prefix, opt_args));
    }

}; // end of specialization
//............................................................................

template<typename LINK_TYPE, typename BUFFER_TAG>
struct mcast_link_factory
{
    /*
     * mcast recv
     */
    static std::unique_ptr<LINK_TYPE> create (std::string const & name_prefix,
                                              std::string const & ifc, std::vector<net::mcast_source> const & sources, capacity_t const capacity,
                                              arg_map const & opt_args = { })
    {
        recv_arg_map args { { "capacity", capacity } };
        set_buffer_args<typename LINK_TYPE::recv_buffer_tag>::evaluate (args, name_prefix, opt_args);

         return std::make_unique<LINK_TYPE> (ifc, sources, args);
    }

    /*
     * mcast send
     */
    static std::unique_ptr<LINK_TYPE> create (std::string const & name_prefix,
                                              std::string const & ifc, capacity_t const capacity,
                                              arg_map const & opt_args = { })
    {
        send_arg_map args { { "capacity", capacity } };
        set_buffer_args<typename LINK_TYPE::send_buffer_tag>::evaluate (args, name_prefix, opt_args);

        return std::make_unique<LINK_TYPE> (ifc, args);
    }

}; // end of metafunction
//............................................................................

template<typename LINK_TYPE>
struct tcp_link_factory
{
    /*
     * client
     */
    static std::unique_ptr<LINK_TYPE> create (std::string const & name_prefix,
                                              std::string const & server, std::string const & service, capacity_t const capacity,
                                              arg_map const & opt_args = { })
    {
        recv_arg_map recv_args { { "capacity", capacity } };
        set_buffer_args<typename LINK_TYPE::recv_buffer_tag>::evaluate (recv_args, name_prefix, opt_args);

        send_arg_map send_args { { "capacity", capacity } };
        set_buffer_args<typename LINK_TYPE::send_buffer_tag>::evaluate (send_args, name_prefix, opt_args);

        return std::make_unique<LINK_TYPE> (server, service, recv_args, send_args);
    }

    /*
     * server
     */
    static std::unique_ptr<LINK_TYPE> create (std::string const & name_prefix,
                                              net::socket_handle && accepted, capacity_t const capacity,
                                              arg_map const & opt_args = { })
    {
        recv_arg_map recv_args { { "capacity", capacity } };
        set_buffer_args<typename LINK_TYPE::recv_buffer_tag>::evaluate (recv_args, name_prefix, opt_args);

        send_arg_map send_args { { "capacity", capacity } };
        set_buffer_args<typename LINK_TYPE::send_buffer_tag>::evaluate (send_args, name_prefix, opt_args);

        return std::make_unique<LINK_TYPE> (std::move (accepted), recv_args, send_args);
    }

}; // end of metafunction

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
