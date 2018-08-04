
#include "vr/io/frame_streams.h"

#include "vr/io/csv/CSV_frame_streams.h"
#include "vr/io/files.h"
#include "vr/io/hdf/HDF_frame_streams.h"

#include <fstream>

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
//............................................................................
//............................................................................
namespace
{

VR_ASSUME_COLD format::enum_t
infer_format (fs::path const & file, arg_map const & parms)
{
    format::enum_t fmt { format::HDF }; // default

    // if 'parms' specifies "format", it overrides everything:

    if (parms.count ("format"))
        fmt = parms.get<format> ("format");
    else // try to infer from 'file' extension, if any:
    {
        switch (str_hash_32 (file.extension ().string ()))
        {
            case ".csv"_hash: fmt = format::CSV; break;
            case ".hdf"_hash: fmt = format::HDF; break;

        } // end of switch
    }

    return fmt;
}

} // end of anonymous
//............................................................................
//............................................................................

std::unique_ptr<frame_istream>
frame_istream::open (fs::path const & file, arg_map const & parms)
{
    format::enum_t const fmt = infer_format (file, parms);

    switch (fmt)
    {
        case format::CSV:
        {
            using stream            = CSV_frame_istream<std::ifstream>;

            fs::path full_file { file };
            if (file.extension () != ".csv") full_file += ".csv";

            return std::unique_ptr<frame_istream> { new stream { parms, full_file.c_str (), default_istream_mode () } };
        }
        /* no break */

        case format::HDF:
        {
            using stream            = HDF_frame_istream<>;

            fs::path full_file { file };
            if (file.extension () != ".hdf") full_file += ".hdf";

            return std::unique_ptr<frame_istream> { new stream { parms, full_file } };
        }
        /* no break */

        default: throw_x (invalid_input, "unsupported frame stream format " + print (fmt));

    } // end of switch
}
//............................................................................

std::unique_ptr<frame_ostream>
frame_ostream::create (fs::path const & file, arg_map const & parms, fs::path * const out_file)
{
    format::enum_t const fmt = infer_format (file, parms);
    clobber::enum_t const cm = parms.get<clobber> ("clobber", clobber::retain);

    switch (fmt)
    {
        case format::CSV:
        {
            using stream            = CSV_frame_ostream<std::ofstream>;

            fs::path full_file { file };
            if (file.extension () != ".csv") full_file += ".csv";

            auto const mode = check_ostream_mode (full_file, cm); // this may throw on cm 'retain/error'
            std::unique_ptr<frame_ostream> r { new stream { parms, full_file.c_str (), mode } };

            if (out_file) (* out_file) = full_file; // communicate the underlying filename if requested
            return r;
        }
        /* no break */

        case format::HDF:
        {
            using stream            = HDF_frame_ostream<>;

            fs::path full_file { file };
            if (file.extension () != ".hdf") full_file += ".hdf";

            auto const mode = check_ostream_mode (full_file, cm); // this may throw on cm 'retain/error'
            std::unique_ptr<frame_ostream> r { new stream { parms, full_file, (mode & std::ios_base::trunc ? O_TRUNC : 0U) } };

            if (out_file) (* out_file) = full_file; // communicate the underlying filename if requested
            return r;
        }
        /* no break */

        default: throw_x (invalid_input, "unsupported frame stream format " + print (fmt));

    } // end of switch
}

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
