#pragma once

#include "vr/arg_map.h"

#include <boost/iostreams/concepts.hpp>

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
class uri; // forward
/**
 */
class HTTP_source: public boost::iostreams::source
{
    private: // ..............................................................

        using super         = boost::iostreams::source;

    public: // ...............................................................

        static constexpr std::streamsize buffer_size_hint ()    { return 16 * 1024; } // same as CURL_MAX_WRITE_SIZE

        struct category: super::category,
                         boost::iostreams::optimally_buffered_tag
        { };


        HTTP_source (uri const & url, arg_map const & parms = { });
        ~HTTP_source (); // calls 'close()'


        // boost::iostreams source:

        static std::streamsize optimal_buffer_size ();

        VR_ASSUME_HOT std::streamsize read (char * const dst, std::streamsize const n);

        VR_ASSUME_COLD void close (); // idempotent

    private: // ..............................................................

        class pimpl;

        std::shared_ptr<pimpl> m_impl; // note: can't use unique ptr because boost::iostream uses copy-construction

}; // end of class

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
