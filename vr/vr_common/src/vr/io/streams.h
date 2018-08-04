#pragma once

#include "vr/asserts.h"
#include "vr/io/defs.h"
#include "vr/io/http/HTTP_source.h"
#include "vr/io/tty/eol_filters.h"
#include "vr/io/zstd/zstd_filters.h"
#include "vr/util/traits.h"

#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/device/file_descriptor.hpp>
#include <boost/iostreams/device/null.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/filter/bzip2.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/iostreams/stream.hpp>

#include <iostream>

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
//............................................................................

template<typename IS>
void
setup_istream (IS & is, io::exceptions)
{
    is.exceptions (std::ios::badbit); // excluding 'eofbit' and 'failbit' by design
}

template<typename IS>
void
setup_istream (IS & is, io::no_exceptions)
{
    is.exceptions (std::ios::goodbit);
}


template<typename OS>
void
setup_ostream (OS & os, io::exceptions)
{
    os.exceptions (std::ios::badbit | std::ios::failbit);
}

template<typename OS>
void
setup_ostream (OS & os, io::no_exceptions)
{
    os.exceptions (std::ios::goodbit);
}
//............................................................................
/**
 * non-intrusively queries a <em>seekable</em> 'is' for its data length
 *
 * @return length of data in 'is' [-1 on EOF]
 */
extern int64_t
istream_length (std::istream & is);

//............................................................................

inline int64_t
read_fully (std::istream & is, int64_t const size, addr_t const dst)
{
    int64_t r { };

    while (is)
    {
        is.read (char_ptr_cast (dst) + r, size - r);
        r += is.gcount ();

        if (VR_LIKELY (r == size)) break;
    }

    return r;
}

inline int64_t
read_until_eof (std::istream & is, char * const buf, int32_t const buf_size, std::ostream & dst)
{
    int64_t r { };

    while (is)
    {
        is.read (buf, buf_size);
        auto const rc = is.gcount ();

        dst.write (buf, rc);
        r += rc;
    }

    return r;
}
//............................................................................

inline int64_t
write_fully (addr_const_t const src, int64_t const size, std::ostream & os)
{
    int64_t r { };
    auto const begin_pos = os.tellp ();
    assert_condition (os);
    assert_nonnegative (begin_pos);

    while (os)
    {
        os.write (char_ptr_cast (src) + r, size - r);
        auto const pos = os.tellp ();

        if (VR_LIKELY (pos > begin_pos)) // note: 'pos' can be -1 on failure
            r = (pos - begin_pos);

        if (VR_LIKELY (r == size)) break;
    }

    return r;
}
//............................................................................
//............................................................................
namespace impl
{
namespace bio   = boost::iostreams;

//............................................................................

using input_streambuf       = bio::filtering_streambuf<bio::input>;
using output_streambuf      = bio::filtering_streambuf<bio::output>;

//............................................................................

template<typename EXC_POLICY>
class array_istream: public bio::stream<bio::array_source>
{
    private: // ..............................................................

        using super                 = bio::stream<bio::array_source>;

    public: // ...............................................................

        template<typename T, int32_t N, typename _ = T>
        array_istream (T (& array) [N], // note: non-const to fit it with the boost API signatures for sources
                typename std::enable_if<(sizeof (T) == 1)>::type * = { }) :
            super (char_array_cast (array))
        {
            setup_istream (* this, EXC_POLICY { });
        }

        template<typename T, typename _ = T>
        array_istream (T const * const array, int32_t const size,
                typename std::enable_if<(sizeof (T) == 1)>::type * = { }) :
            super (char_ptr_cast (array), size)
        {
            setup_istream (* this, EXC_POLICY { });
        }

}; // end of class

template<typename EXC_POLICY>
class array_ostream: public bio::stream<bio::array_sink>
{
    private: // ..............................................................

        using super                 = bio::stream<bio::array_sink>;

    public: // ...............................................................

        template<typename T, int32_t N, typename _ = T>
        array_ostream (T (& array) [N],
                typename std::enable_if<(sizeof (T) == 1)>::type * = { }) :
            super (char_array_cast (array))
        {
            setup_ostream (* this, EXC_POLICY { });
        }

        template<typename T, typename _ = T>
        array_ostream (T * const array, int32_t const size,
                typename std::enable_if<(sizeof (T) == 1)>::type * = { }) :
            super (char_ptr_cast (array), size)
        {
            setup_ostream (* this, EXC_POLICY { });
        }

}; // end of class
//............................................................................

class eol_ostream: public std::ostream
{
    public: // ...............................................................

        eol_ostream (std::ostream & os) :
            std::ios (nullptr),
            std::ostream (& m_streambuf),
            m_os { os },
            m_streambuf { eol_filter { bio::newline::dos } }
        {
            m_streambuf.push (m_os);
        }

        /**
         * @return the underlying unfiltered stream as passed into the constructor
         */
        std::ostream & os ()
        {
            return m_os;
        }

    private: // ..............................................................

        std::ostream & m_os;
        output_streambuf m_streambuf;

}; // end of class
//............................................................................

template<typename EXC_POLICY>
class fd_istream: public bio::stream<bio::file_descriptor_source>
{
    private: // ..............................................................

        using super                 = bio::stream<bio::file_descriptor_source>;

    public: // ...............................................................

        template<typename PATH>
        fd_istream (PATH const & path, std::ios_base::openmode const mode = default_istream_mode ()) :
            super (path, mode)
        {
            setup_istream (* this, EXC_POLICY { });
        }

        fd_istream (int32_t const fd, bool const close_fd_on_destruct) :
            super (fd, (close_fd_on_destruct ? bio::close_handle : bio::never_close_handle))
        {
            setup_istream (* this, EXC_POLICY { });
        }


        // ACCESSORs:

        int32_t fd () const
        {
            // this is a little convoluted due to operator overloadings in 'bio::stream':

            super const * stream { this };

            return (* const_cast<super *> (stream))->handle ();
        }

}; // end of class

template<typename EXC_POLICY>
class fd_ostream: public bio::stream<bio::file_descriptor_sink>
{
    private: // ..............................................................

        using super                 = bio::stream<bio::file_descriptor_sink>;

    public: // ...............................................................

        template<typename PATH>
        fd_ostream (PATH const & path, std::ios_base::openmode const mode = default_ostream_mode ()) :
            super (path, mode)
        {
            setup_ostream (* this, EXC_POLICY { });
        }

        fd_ostream (int32_t const fd, bool const close_fd_on_destruct) :
            super (fd, (close_fd_on_destruct ? bio::close_handle : bio::never_close_handle))
        {
            setup_ostream (* this, EXC_POLICY { });
        }


        // ACCESSORs:

        int32_t fd () const
        {
            // this is a little convoluted due to operator overloadings in 'bio::stream':

            super const * stream { this };

            return (* const_cast<super *> (stream))->handle ();
        }

}; // end of class
//............................................................................

template<typename EXC_POLICY>
class http_istream: public bio::stream<HTTP_source>
{
    private: // ..............................................................

        using super                 = bio::stream<HTTP_source>;

    public: // ...............................................................

        static constexpr std::streamsize buffer_size_hint ()    { return HTTP_source::buffer_size_hint (); }

        template<typename ... ARGs>
        http_istream (ARGs && ... args) :
            super (std::forward<ARGs> (args) ...)
        {
            setup_istream (* this, EXC_POLICY { });
        }

}; // end of class
//............................................................................

template<typename IS, typename EXC_POLICY>
class zlib_istream: public std::istream
{
    public: // ...............................................................

        template<typename ... ARGs>
        zlib_istream (ARGs && ... args) :
            std::ios (nullptr),
            std::istream (& m_streambuf),
            m_is { std::forward<ARGs> (args) ... },
            m_streambuf { bio::zlib_decompressor { } }
        {
            m_streambuf.push (m_is);

            // TODO apply EXC_POLICY to 'm_is'
            setup_istream (* this, EXC_POLICY { });
        }

    private: // ..............................................................

        IS m_is;
        input_streambuf m_streambuf;

}; // end of class

template<typename OS, typename EXC_POLICY>
class zlib_ostream: public std::ostream
{
    public: // ...............................................................

        template<typename ... ARGs>
        zlib_ostream (ARGs && ... args) :
            std::ios (nullptr),
            std::ostream (& m_streambuf),
            m_os { std::forward<ARGs> (args) ... },
            m_streambuf { bio::zlib_compressor { } }
        {
            m_streambuf.push (m_os);

            setup_ostream (* this, EXC_POLICY { });
            // TODO apply EXC_POLICY to 'm_os'
        }

    private: // ..............................................................

        OS m_os;
        output_streambuf m_streambuf;

}; // end of class
//............................................................................

template<typename IS, typename EXC_POLICY>
class gzip_istream: public std::istream
{
    public: // ...............................................................

        template<typename ... ARGs>
        gzip_istream (ARGs && ... args) :
            std::ios (nullptr),
            std::istream (& m_streambuf),
            m_is { std::forward<ARGs> (args) ... },
            m_streambuf { bio::gzip_decompressor { } }
        {
            m_streambuf.push (m_is);

            // TODO apply EXC_POLICY to 'm_is'
            setup_istream (* this, EXC_POLICY { });
        }

    private: // ..............................................................

        IS m_is;
        input_streambuf m_streambuf;

}; // end of class

template<typename OS, typename EXC_POLICY>
class gzip_ostream: public std::ostream
{
    public: // ...............................................................

        template<typename ... ARGs>
        gzip_ostream (ARGs && ... args) :
            std::ios (nullptr),
            std::ostream (& m_streambuf),
            m_os { std::forward<ARGs> (args) ... },
            m_streambuf { bio::gzip_compressor { } }
        {
            m_streambuf.push (m_os);

            setup_ostream (* this, EXC_POLICY { });
            // TODO apply EXC_POLICY to 'm_os'
        }

    private: // ..............................................................

        OS m_os;
        output_streambuf m_streambuf;

}; // end of class
//............................................................................

template<typename IS, typename EXC_POLICY>
class bzip2_istream: public std::istream
{
    public: // ...............................................................

        template<typename ... ARGs>
        bzip2_istream (ARGs && ... args) :
            std::ios (nullptr),
            std::istream (& m_streambuf),
            m_is { std::forward<ARGs> (args) ... },
            m_streambuf { bio::bzip2_decompressor { } }
        {
            m_streambuf.push (m_is);

            // TODO apply EXC_POLICY to 'm_is'
            setup_istream (* this, EXC_POLICY { });
        }

    private: // ..............................................................

        IS m_is;
        input_streambuf m_streambuf;

}; // end of class

template<typename OS, typename EXC_POLICY>
class bzip2_ostream: public std::ostream
{
    public: // ...............................................................

        template<typename ... ARGs>
        bzip2_ostream (ARGs && ... args) :
            std::ios (nullptr),
            std::ostream (& m_streambuf),
            m_os { std::forward<ARGs> (args) ... },
            m_streambuf { bio::bzip2_compressor { } }
        {
            m_streambuf.push (m_os);

            setup_ostream (* this, EXC_POLICY { });
            // TODO apply EXC_POLICY to 'm_os'
        }

    private: // ..............................................................

        OS m_os;
        output_streambuf m_streambuf;

}; // end of class
//............................................................................

template<typename IS, typename EXC_POLICY>
class zstd_istream: public std::istream
{
    public: // ...............................................................

        template<typename ... ARGs>
        zstd_istream (ARGs && ... args) :
            std::ios (nullptr),
            std::istream (& m_streambuf),
            m_is { std::forward<ARGs> (args) ... },
            m_streambuf { zstd_decompressor<> { /* TODO parms */ } }
        {
            m_streambuf.push (m_is);

            // TODO apply EXC_POLICY to 'm_is'
            setup_istream (* this, EXC_POLICY { });
        }

    private: // ..............................................................

        IS m_is;
        input_streambuf m_streambuf;

}; // end of class

template<typename OS, typename EXC_POLICY>
class zstd_ostream: public std::ostream
{
    public: // ...............................................................

        template<typename ... ARGs>
        zstd_ostream (ARGs && ... args) :
            std::ios (nullptr),
            std::ostream (& m_streambuf),
            m_os { std::forward<ARGs> (args) ... },
            m_streambuf { zstd_compressor<> { /* TODO parms */ } }
        {
            m_streambuf.push (m_os);

            setup_ostream (* this, EXC_POLICY { });
            // TODO apply EXC_POLICY to 'm_os'
        }

    private: // ..............................................................

        OS m_os;
        output_streambuf m_streambuf;

}; // end of class

}; // end of 'impl'
//............................................................................
//............................................................................

using null_ostream                  = impl::bio::stream<impl::bio::null_sink>;

//............................................................................

template<typename EXC_POLICY = io::exceptions>
using array_istream                 = impl::array_istream<EXC_POLICY>;

template<typename EXC_POLICY = io::exceptions>
using array_ostream                 = impl::array_ostream<EXC_POLICY>;

//............................................................................

template<typename EXC_POLICY = io::exceptions>
using fd_istream                    = impl::fd_istream<EXC_POLICY>;

template<typename EXC_POLICY = io::exceptions>
using fd_ostream                    = impl::fd_ostream<EXC_POLICY>;

//............................................................................

template<typename EXC_POLICY = io::exceptions>
using http_istream                  = impl::http_istream<EXC_POLICY>;

//............................................................................

using eol_ostream                   = impl::eol_ostream;

//............................................................................

template<typename IS, typename EXC_POLICY = io::exceptions>
using zlib_istream                 = impl::zlib_istream<IS, EXC_POLICY>;

template<typename OS, typename EXC_POLICY = io::exceptions>
using zlib_ostream                 = impl::zlib_ostream<OS, EXC_POLICY>;

//............................................................................
/**
 * @code
 * #include <fstream>
 * ...
 *  gzip_ostream<std::ofstream> out { "file.gz", (std::ios_base::out | std::ios_base::binary | std::ios_base::trunc) };
 *  out.write (...)
 * @endcode
 */
template<typename IS, typename EXC_POLICY = io::exceptions>
using gzip_istream                  = impl::gzip_istream<IS, EXC_POLICY>;

/**
 * @code
 * #include <fstream>
 * ...
 *  gzip_istream<std::ifstream> in { "file.gz", (std::ios_base::in | std::ios_base::binary) };
 *  while (in.read (...))
 *  {
 *      ...
 *  }
 * @endcode
 */
template<typename OS, typename EXC_POLICY = io::exceptions>
using gzip_ostream                  = impl::gzip_ostream<OS, EXC_POLICY>;

//............................................................................

template<typename IS, typename EXC_POLICY = io::exceptions>
using bzip2_istream                 = impl::bzip2_istream<IS, EXC_POLICY>;

template<typename OS, typename EXC_POLICY = io::exceptions>
using bzip2_ostream                 = impl::bzip2_ostream<OS, EXC_POLICY>;

//............................................................................

template<typename IS, typename EXC_POLICY = io::exceptions>
using zstd_istream                 = impl::zstd_istream<IS, EXC_POLICY>;

template<typename OS, typename EXC_POLICY = io::exceptions>
using zstd_ostream                 = impl::zstd_ostream<OS, EXC_POLICY>;

//............................................................................

template<compression::enum_t FORMAT>
struct stream_for; // master

#define vr_SPECIALIZE(r, unused, FORMAT)\
\
    template<> \
    struct stream_for<compression:: FORMAT > \
    { \
        template<typename IS, typename EXC_POLICY = io::exceptions> \
        using istream                   = BOOST_PP_CAT (FORMAT, _istream)<IS, EXC_POLICY>; \
        \
        template<typename OS, typename EXC_POLICY = io::exceptions> \
        using ostream                   = BOOST_PP_CAT (FORMAT, _ostream)<OS, EXC_POLICY>; \
        \
    }; \
    /* */

    BOOST_PP_SEQ_FOR_EACH (vr_SPECIALIZE, unused, BOOST_PP_SEQ_TAIL (VR_IO_COMPRESSION_SEQ))

#undef vr_SPECIALIZE

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
