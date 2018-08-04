
#include "vr/io/stream_factory.h"

#include "vr/io/files.h"
#include "vr/io/streams.h"
#include "vr/str_hash.h"
#include "vr/util/logging.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
//............................................................................
//............................................................................
namespace
{

VR_ASSUME_COLD compression::enum_t
infer_compression (fs::path const & path, arg_map const & parms, bool const check_content)
{
    compression::enum_t cmprss { compression::none }; // default

    // if 'parms' specifies "compression", it overrides everything:

    if (parms.count ("compression"))
        cmprss = parms.get<compression> ("compression");
    else // try to infer from 'path' extension, if any, and/or content:
        cmprss = guess_compression (path, check_content);

    return cmprss;
}

} // end of anonymous
//............................................................................
//............................................................................

// TODO support exception policy selection
// TODO autoregistration/pluggability arch

std::unique_ptr<std::istream>
stream_factory::open_input (fs::path const & path, arg_map const & parms)
{
    LOG_trace1 << "opening " + print (path) + " for input ...";

    {
        check_nonempty (path);
        compression::enum_t const cmprss = infer_compression (path, parms, true);

        LOG_trace1 << "scheme: \"file\", compression: " << print (cmprss);

        try
        {
            switch (cmprss)
            {
                case compression::none: return std::make_unique<fd_istream<> > (path);

#           define vr_CASE(r, unused, FORMAT) \
                case compression:: FORMAT : return std::make_unique< BOOST_PP_CAT (FORMAT, _istream)<fd_istream<> > > (path); \
                /* */

                BOOST_PP_SEQ_FOR_EACH (vr_CASE, unused, BOOST_PP_SEQ_TAIL (VR_IO_COMPRESSION_SEQ))

#           undef vr_CASE

                default: break;

            } // end of switch
        }
        catch (std::exception const & e) // intercept to report 'path'
        {
            chain_x (io_exception, "failed to open " + print (path));
        }

        throw_x (invalid_input, "can't infer stream type for file compression " + print (cmprss));
    }
}

std::unique_ptr<std::istream>
stream_factory::open_input (uri const & url, arg_map const & parms)
{
    LOG_trace1 << "opening " + print (url) + " for input ...";

    uri::components const url_parts = url.split ();

    auto const scheme = url_parts.scheme ();
    fs::path const path { url_parts.path ().to_string () };

    switch (str_hash_32 (scheme.m_start, scheme.m_size))
    {
        case "file"_hash:
        {
            return open_input (path, parms);
        }
        /* no break */


        case  "http"_hash:
        case "https"_hash:
        {
            compression::enum_t const cmprss = infer_compression (path, parms, /* don't peek at magic numbers in remote content */false);
            LOG_trace1 << "scheme: " << print (scheme) << ", compression: " << print (cmprss);

            try
            {
                switch (cmprss)
                {
                    case compression::none: return std::make_unique<http_istream<> > (url, parms);

#               define vr_CASE(r, unused, FORMAT) \
                    case compression:: FORMAT : return std::make_unique< BOOST_PP_CAT (FORMAT, _istream)<http_istream<> > > (url, parms); \
                    /* */

                    BOOST_PP_SEQ_FOR_EACH (vr_CASE, unused, BOOST_PP_SEQ_TAIL (VR_IO_COMPRESSION_SEQ))

#               undef vr_CASE

                    default: break;

                } // end of switch
            }
            catch (std::exception const & e) // intercept to report 'path'
            {
                chain_x (io_exception, "failed to open " + print (url));
            }

            throw_x (invalid_input, "can't infer stream type for scheme " + print (scheme) + " and compression " + print (cmprss));
        }
        /* no break */

    } // end of switch

    throw_x (invalid_input, "unsupported scheme " + print (scheme));
}

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
