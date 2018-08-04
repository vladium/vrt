#pragma once

#include "vr/filesystem.h"
#include "vr/io/defs.h" // 'filter'
#include "vr/util/datetime.h"
#include "vr/util/json_fwd.h"
#include "vr/utility.h" // optional

#include <boost/regex_fwd.hpp>

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
class uri; // forward

//............................................................................

extern fs::path
absolute_path (fs::path const & path, fs::path const & base = { });

/**
 * like the classic canonical_path() but accepts paths that don't yet fully exist
 */
extern fs::path
weak_canonical_path (fs::path const & path, fs::path const & base = { });

//............................................................................
/**
 * for design symmetry with 'file_size(int32_t fd)'
 */
inline signed_size_t
file_size (fs::path const & path)
{
    return fs::file_size (path);
}

/**
 * an alterantive to 'file_size()' when you already have a file descriptor
 * and would like to avoid another filesystem path lookup
 */
extern signed_size_t
file_size (int32_t const fd);

//............................................................................
/**
 * a non-intrusive test whether 'fd' is a valid file descriptor
 */
extern bool
fd_valid (int32_t const fd);

/**
 * @return 'true' if 'path' can be interpreted as an *existing* pathname
 *         relative to 'base' as interpreted by @ref absolute_path(),
 *         including symlink resolution if any
 *
 * @note the reason a mere 'fs::exists()' is not enough is because it can throw
 *       on arbitrary strings (e.g. if the length is longer that the OS max, etc)
 */
extern bool
path_valid (fs::path const & path, fs::path const & base = { });

inline bool
path_valid (std::string const & path, fs::path const & base = { })
{
    return path_valid (fs::path { path }, base);
}
//............................................................................
/**
 * a non-root "surgical" method for removing 'fd' pages from the system cache
 *
 * @note this uses 'posix_fadvise()', which is not binding
 */
extern bool
drop_from_cache (int32_t const fd);
/**
 * a non-root "surgical" method for removing 'file' pages from the system cache
 *
 * @note this uses 'posix_fadvise()', which is not binding
 */
extern bool
drop_from_cache (fs::path const & file);

//............................................................................

extern string_vector
read_lines (fs::path const & path);

/**
 * @param s [if starts with '@' the rest will be interpreted as a filepath]
 *
 * @return a vector containing either the single 's' or the result of 'read_lines (s.substr (1))'
 */
extern string_vector
read_lines_string_or_atfile (std::string const & s);

/**
 * a multi-input version of 'read_lines_string_or_atfile ()'
 */
extern string_vector
read_lines_strings_or_atfiles (string_vector const & ss);

//............................................................................

extern json
read_json (fs::path const & file);

extern json
read_json (uri const & url);

//............................................................................

extern fs::path
temp_dir ();

//............................................................................
/**
 * @param filename_regex filter to apply to the 'filename()' component of every
 *        file directly under 'dir' [empty means 'match all']
 *
 * @return list of matches [paths will be absolute/relative according to
 *         boost::directory_iterator rules]
 */
extern std::vector<fs::path>
list_files (fs::path const & dir, boost::regex const & filename_regex = boost::regex { });

//............................................................................
/*
 * TODO this can almost be replicated with list_files()
 */
/**
 * @throw invalid_input if 'filename_regex' matches more than one file in 'dir'
 */
extern optional<fs::path>
find_file (boost::regex const & filename_regex, fs::path const & dir);

extern optional<fs::path>
find_path_recursive (fs::path const & path, fs::path const & search_root);

//............................................................................
/**
 * @return a unique path ending in "<segment to ensure uniqueness><name_suffix>"
 *
 * @note the resulting path will be rooted at 'parent' but is not guaranteed to
 *       exist, use @see create_dir() or @see create_sparse_file() for that
 */
extern fs::path
unique_path (fs::path const & parent, std::string const & name_suffix = { });

/**
 * equivalent to
 * @code
 *  unique_path(temp_dir () / VR_APP_NAME, name_suffix)
 * @endcode
 */
extern fs::path
unique_temp_path (std::string const & name_suffix = { });

//............................................................................
/**
 * similar to @ref unique_path except it uses '::mkstemp()' to do the sequence
 * {generate unique filename, create it, open it} "atomically"
 */
extern std::pair<fs::path, int32_t>
open_unique_file (fs::path const & parent, std::string const & name_suffix = { });

//............................................................................
/**
 *
 * @see util::format_date(), this is overlaps somewhat
 */
extern fs::path
date_specific_path (std::string const & model, util::date_t const & date);

inline fs::path
date_specific_path (fs::path const & model, util::date_t const & date)
{
    return date_specific_path (model.string (), date);
}
//............................................................................
/**
 * @param dir [may not be empty]
 * @return 'true' if some new directory creation was involved
 *
 * @throws io_exception on failure
 */
extern bool
create_dirs (fs::path const & dir);

/**
 * @throws io_exception on failure
 */
extern void
create_sparse_file (fs::path const & file, int64_t const size = 0);

//............................................................................
/**
 * removes all contents of 'dir', recursively; if 'remove' is 'true', removes
 * 'dir' as well
 *
 * @throws io_exception on failure
 */
extern void
clean_dir (fs::path const & dir, bool const remove = false);

//............................................................................
/**
 * @throws operation_redundant if 'file' exists and 'cm' is 'retain'
 * @throws operation_not_allowed if it is an error for 'file' to exist according to 'cm'
 */
extern std::ios_base::openmode
check_ostream_mode (fs::path const & file, clobber::enum_t const cm, bool const file_exists);
/**
 * equivalent to 'check_ostream_mode (file, cm, fs::exists (file))', useful when file existence
 * check has already been done
 */
extern std::ios_base::openmode
check_ostream_mode (fs::path const & file, clobber::enum_t const cm);

//............................................................................
/**
 * @note if 'check_content' is 'false' (false) this relies on 'path' extension
 * entirely, no actual I/O is done; otherwise the implementation may read a small
 * number of bytes ('magic') from 'path' to confirm the format (assuming 'path'
 * exists and is readable)
 *
 * @throws io_exception if the extension and content (if allowed) seriously contradict
 *         each other
 */
extern compression::enum_t
guess_compression (fs::path const & path, bool const check_content = false);

//............................................................................

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
