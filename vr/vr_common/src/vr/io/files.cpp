
#include "vr/io/files.h"

#include "vr/asserts.h"
#include "vr/io/exceptions.h"
#include "vr/io/streams.h"
#include "vr/io/stream_factory.h"
#include "vr/strings.h"
#include "vr/str_hash.h"
#include "vr/sys/os.h"
#include "vr/util/json.h"
#include "vr/util/logging.h"

#include <boost/algorithm/string/trim.hpp>
#include <boost/system/error_code.hpp>

#include <fstream>
#include <locale>

#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
//............................................................................

fs::path
absolute_path (fs::path const & path, fs::path const & base)
{
    return fs::absolute (path, (base.empty () ? fs::current_path () : base));
}

fs::path
weak_canonical_path (fs::path const & path, fs::path const & base)
{
    return fs::weakly_canonical (absolute_path (path, base));
}
//............................................................................

signed_size_t
file_size (int32_t const fd)
{
    check_nonnegative (fd);

    struct ::stat sb { };
    VR_CHECKED_SYS_CALL (::fstat (fd, & sb));

    return sb.st_size;
}
//............................................................................

bool
fd_valid (int32_t const fd)
{
    if (fd < 0) return false; // fast fail

    return ! (::fcntl (fd, F_GETFL) < 0 && (errno == EBADF));
}

bool
path_valid (fs::path const & path, fs::path const & base)
{
    fs::path const p = absolute_path (path, base);
    boost::system::error_code ec { };

    fs::file_status const st = fs::status (p, ec); // doesn't throw
    if (ec) return false;

    if (! fs::exists (st))
        return false;

    return (fs::is_regular_file (st) || fs::is_directory (st) || fs::is_symlink (st));
}
//............................................................................

bool
drop_from_cache (int32_t const fd)
{
    check_nonnegative (fd);

    return (! ::posix_fadvise (fd, 0, 0, POSIX_FADV_DONTNEED));
}

bool
drop_from_cache (fs::path const & file)
{
    fd_istream<io::no_exceptions> in { file }; // TODO something more lightweight

    return drop_from_cache (in.fd ());
}
//............................................................................

string_vector
read_lines (fs::path const & path)
{
    std::ifstream is { path.c_str (), (std::ios_base::in | std::ios_base::binary ) };

    string_vector r { };

    std::string line { };
    while (is)
    {
        std::getline (is, line);

        if (line.empty () || (line [0] == '#')) continue; // skip empty lines, comments

        boost::algorithm::trim (line); // in-place
        r.push_back (line);
    }

    return r;
}

string_vector
read_lines_string_or_atfile (std::string const & s)
{
    if (s.size () > 1 && s [0] == '@')
        return read_lines (s.substr (1));

    return { s };
}

string_vector
read_lines_strings_or_atfiles (string_vector const & ss)
{
    string_vector r { };

    for (std::string const & s : ss)
    {
        string_vector s_lines { read_lines_string_or_atfile (s) };
        string_vector && lines { std::move (s_lines) };

        std::move (lines.begin (), lines.end (), r.end ()); // last reference to 's_lines/lines'
    }

    return r;
}
//............................................................................

json
read_json (fs::path const & file)
{
    std::unique_ptr<std::istream> const in { stream_factory::open_input (file) };

    return json::parse (* in);
}

json
read_json (io::uri const & url)
{
    std::unique_ptr<std::istream> const in { stream_factory::open_input (url) };

    return json::parse (* in);
}
//............................................................................

fs::path
temp_dir ()
{
    return fs::temp_directory_path ();
}
//............................................................................

std::vector<fs::path>
list_files (fs::path const & dir, boost::regex const & filename_regex)
{
    std::vector<fs::path> r { };

    // TODO use native glob/fnmatch for better performance

    for (fs::directory_iterator i { dir }; i != fs::directory_iterator { }; ++ i)
    {
        fs::path const & p = i->path ();

        if (filename_regex.empty () || boost::regex_match (p.filename ().native (), filename_regex))
            r.push_back (p);
    }

    return r;
}
//............................................................................

optional<fs::path>
find_file (boost::regex const & filename_regex, fs::path const & dir)
{
    optional<fs::path> r;

    for (fs::directory_entry const & e : fs::directory_iterator { dir })
    {
        fs::path const & p = e.path ();

        if (boost::regex_match (p.filename ().native (), filename_regex))
        {
            if (VR_UNLIKELY (!! r))
                throw_x (invalid_input, print (dir) + ": multiple matching files found, " + print (p.filename ()) + " and " + print (r.get ().filename ()));

            r = absolute_path (p, dir);
        }
    }

    return r;
}

optional<fs::path>
find_path_recursive (fs::path const & path, fs::path const & search_root)
{
    LOG_trace1 << "find_path_recursive(" << print (search_root) << ", " << print (path) << "): entry";

    if (path.is_absolute () && path_valid (path))
    {
        LOG_trace1 << "find_path_recursive(" << print (search_root) << ", " << print (path) << "): exit, returning absolute path sought";
        return path;
    }

    assert_condition (path.is_relative (), path);

    fs::path const first = * path.begin (); // note: copy [path iterators return refs to objects that don't outlive the iterators themselves]

    for (fs::directory_entry const & e : fs::recursive_directory_iterator { search_root })
    {
        if (e.path ().filename () == first)
        {
            LOG_trace2 << "trying " << print (e.path ()) << " ...";

            // the rest of the sought-for path is now a non-recursive check:

            fs::path const r { absolute_path (path, e.path ().parent_path ()) };

            if (path_valid (r))
            {
                LOG_trace1 << "find_path_recursive(" << print (search_root) << ", " << print (path) << "): exit, found " << print (r);
                return { r };
            }
            // else continue recursing
        }
    }

    LOG_trace1 << "find_path_recursive(" << print (search_root) << ", " << print (path) << "): exit, no match";
    return { };
}
//............................................................................

#define vr_UNIQUE_PREFIX    VR_APP_NAME ".%%%%%%"

fs::path
unique_path (fs::path const & parent, std::string const & name_suffix)
{
    return fs::unique_path (parent / join_as_name (vr_UNIQUE_PREFIX, name_suffix));
}

fs::path
unique_temp_path (std::string const & name_suffix)
{
    return unique_path (temp_dir () / VR_APP_NAME, name_suffix);
}
//............................................................................

#define vr_MKTEMP_SEGMENT   "XXXXXX"

std::pair<fs::path, int32_t>
open_unique_file (fs::path const & parent, std::string const & name_suffix)
{
    std::string path { (parent / join_as_name (VR_APP_NAME "." vr_MKTEMP_SEGMENT, name_suffix)).string () };

    auto const xs = path.find (vr_MKTEMP_SEGMENT);
    check_ne (xs, std::string::npos);

    // [note: std::string::data () is null-terminated in c++11]

    int32_t const fd = VR_CHECKED_SYS_CALL (::mkstemps (const_cast<char *> (path.data ()), path.size () - xs - sizeof (vr_MKTEMP_SEGMENT) + 1));

    return { path, fd };
}
//............................................................................

fs::path
date_specific_path (std::string const & model, util::date_t const & date)
{
    std::stringstream ss;
    ss.imbue (std::locale { ss.getloc (), new gd::date_facet { model.c_str () } });

    ss << date;

    return { ss.str () };
}
//............................................................................

bool
create_dirs (fs::path const & dir)
{
    check_condition (! dir.empty ());

    fs::file_status const s { fs::status (dir) };

    if (fs::exists (s))
    {
        if (fs::is_directory (s))
            return false; // nothing to do

        throw_x (io_exception, "path " + print (absolute_path (dir)) + " exists but is not a directory");
    }

    assert_condition (! fs::exists (s));

    try
    {
        return fs::create_directories (dir);
    }
    catch (...)
    {
        chain_x (io_exception, "create_dirs(" + print (absolute_path (dir)) + ") failed");
    }
}

void
create_sparse_file (fs::path const & file, int64_t const size)
{
    check_condition (! file.empty ());

    try
    {
        fs::file_status const s { fs::status (file) };

        if (fs::exists (s))
        {
            if (fs::is_regular_file (s))
                fs::resize_file (file, size);
            else
                throw_x (io_exception, "path " + print (absolute_path (file)) + " exists but is not a regular file");
        }
        else
        {
            create_dirs (file.parent_path ());
            std::fstream _ { file.c_str (), (std::ios_base::in | std::ios_base::out | std::ios_base::trunc) };
        }
    }
    catch (io_exception const &)
    {
        throw; // re-throw
    }
    catch (...)
    {
        chain_x (io_exception, "create_sparse_file(" + print (absolute_path (file)) + ", " + string_cast (size) + ") failed");
    }
}
//............................................................................

void
clean_dir (fs::path const & dir, bool const remove)
{
    check_condition (! dir.empty ());

    try
    {
        fs::file_status const s { fs::status (dir) };

        if (fs::exists (s))
        {
            if (! fs::is_directory (s))
                throw_x (io_exception, "path " + print (absolute_path (dir)) + " exists but is not a directory");
            else
            {
                if (remove)
                    fs::remove_all (dir);
                else
                {
                    for (fs::directory_entry const & de : fs::directory_iterator { dir })
                    {
                        fs::remove_all (de.path ()); // note: this handles symlinks as expected (links deleted, not their targets)
                    }
                }
            }
        }
    }
    catch (io_exception const &)
    {
        throw; // re-throw
    }
    catch (...)
    {
        chain_x (io_exception, "clean_dir(" + print (absolute_path (dir)) + ") failed");
    }
}
//............................................................................

std::ios_base::openmode
check_ostream_mode (fs::path const & file, clobber::enum_t const cm, bool const file_exists)
{
    switch (cm)
    {
        case clobber::retain:
            if (file_exists) throw_x (operation_redundant, "exists: " + print (file));
            return default_ostream_mode ();

        case clobber::trunc:
            return (default_ostream_mode () | (file_exists ? std::ios_base::trunc : zero_stream_mode ()));

        case clobber::error:
            if (file_exists) throw_x (operation_not_allowed, "exists, can't overwrite: " + print (file));
            return default_ostream_mode ();

        default: VR_ASSUME_UNREACHABLE ();

    } // end of switch
}

std::ios_base::openmode
check_ostream_mode (fs::path const & file, clobber::enum_t const cm)
{
    return check_ostream_mode (file, cm, fs::exists (file));
}
//............................................................................
//............................................................................
namespace
{

compression::enum_t
check_file_content (fs::path const & path)
{
    fs::file_status const s { fs::status (path) };

    if (! fs::exists (s))
        return compression::none; // note: not throwing by design

    // TODO check status (regular file, etc)

    union
    {
        uint16_t hdr16;
        uint32_t hdr32;
    };
    {
        fd_istream<io::no_exceptions> is { path }; // note: not throwing by design

        if (read_fully (is, sizeof (hdr32), & hdr32) != sizeof (hdr32))
            return compression::none;
    }

    LOG_trace1 << "read 4-byte magic " << hex_string_cast (hdr32);

    switch (hdr16 & 0xFF)
    {
        case 0x1F: return compression::gzip;
        case 0x78: return compression::zlib;

    } // end of switch

    if (hdr16 == 0x5A42) // 'BZ'
        return compression::bzip2;

    if ((hdr32 & ~0x0F) == 0xFD2FB520)
        return compression::zstd;

    return compression::none;
}

}
//............................................................................
//............................................................................

compression::enum_t
guess_compression (fs::path const & path, bool const check_content)
{
    fs::path const ext = path.extension ();

    compression::enum_t const content_format = (check_content ? check_file_content (path) : compression::none);
    if (check_content) LOG_trace1 << "content check: " << content_format;

    switch (str_hash_32 (ext.string ()))
    {
        case   ".bz"_hash:
        case  ".bz2"_hash:
        case  ".tbz"_hash:
        case ".tbz2"_hash:
        {
            if (check_content)
            {
                if ((content_format != compression::none) & (content_format != compression::bzip2))
                    throw_x (io_exception, "extension '" + ext.string () + "' inconsistent with " + print (content_format) + " content");
            }
        }
        return compression::bzip2;


        case   ".gz"_hash:
        case ".gzip"_hash:
        case  ".tgz"_hash:
        {
            if (check_content)
            {
                // allow some fuzziness b/w 'gzip' and 'zlib':

                switch (content_format)
                {
                    case compression::gzip:
                        return compression::gzip;

                    case compression::zlib:
                    case compression::none:
                        return compression::zlib;

                    default:
                        throw_x (io_exception, "extension '" + ext.string () + "' inconsistent with " + print (content_format) + " content");

                } // end of switch
            }
        }
        return compression::gzip;


        case ".tz"_hash:
        case ".zz"_hash:
        {
            if (check_content)
            {
                // allow some fuzziness b/w 'gzip' and 'zlib':

                switch (content_format)
                {
                    case compression::gzip:
                        return compression::gzip;

                    case compression::zlib:
                    case compression::none:
                        return compression::zlib;

                    default:
                        throw_x (io_exception, "extension '" + ext.string () + "' inconsistent with " + print (content_format) + " content");

                } // end of switch
            }
        }
        return compression::zlib;


        case ".zst"_hash:
        {
            if (check_content)
            {
                if ((content_format != compression::none) & (content_format != compression::zstd))
                    throw_x (io_exception, "extension '" + ext.string () + "' inconsistent with " + print (content_format) + " content");
            }
        }
        return compression::zstd;


        default:
            return (check_content ? content_format : compression::none);

    } // end of switch
}

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
