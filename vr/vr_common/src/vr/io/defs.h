#pragma once

#include "vr/enums.h"
#include "vr/meta/tags.h"

#include <ios>

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
//............................................................................

using pos_t                 = signed_size_t;

// 'pos_t' aliases:

using capacity_t            = pos_t;
using window_t              = pos_t;

//............................................................................

VR_META_TAG (ring);
VR_META_TAG (tape);

//............................................................................

constexpr pos_t default_mmap_reserve_size ()    { return (1L << 40); }

//............................................................................

VR_META_TAG (ts_origin);
VR_META_TAG (ts_local);
VR_META_TAG (ts_local_delta);

VR_META_TAG (ts_last_recv);
VR_META_TAG (ts_last_send);

//............................................................................

constexpr std::ios_base::openmode default_istream_mode ()  { return (std::ios_base::in  | std::ios_base::binary); }
constexpr std::ios_base::openmode default_ostream_mode ()  { return (std::ios_base::out | std::ios_base::binary); }

constexpr std::ios_base::openmode zero_stream_mode ()      { return static_cast<std::ios_base::openmode> (0); }

//............................................................................

VR_EXPLICIT_ENUM (mode, // TODO replace with "bitset enum" macro when/if have those
    enum_int_t,
        (0b00,          none)
        (0b01,          recv)
        (0b10,          send)
    ,
    printable, parsable

); // end of enum
//............................................................................

VR_ENUM (clobber,
    (
        retain,
        trunc,
        error
    ),
    printable, parsable

); // end of enum
//............................................................................

VR_ENUM (advice_dir,
    (
        forward,
        backward
    ),
    printable, iterable

); // end of enum
//............................................................................

#define VR_IO_FORMAT_SEQ    \
        (CSV)               \
        (HDF)               \
    /* */

VR_ENUM (format,
    (
        BOOST_PP_SEQ_ENUM (VR_IO_FORMAT_SEQ)
    ),
    printable, parsable

); // end of enum
//............................................................................

#define VR_IO_COMPRESSION_SEQ   \
        (none)                  \
        (zstd)                  \
        (gzip)                  \
        (zlib)                  \
        (bzip2)                 \
    /* */

struct compression final
{
    VR_NESTED_ENUM (
        (
            BOOST_PP_SEQ_ENUM (VR_IO_COMPRESSION_SEQ)
        ),
        printable, parsable
    )

    static constexpr string_literal_t file_extension [size] =
    {
         "",
         ".zst",
         ".gz" ,
         ".zz" ,
         ".bz2"
    };

}; // end of enum
//............................................................................

VR_ENUM (filter,
    (
        none,
        zlib,
        blosc
    ),
    iterable, printable, parsable

); // end of enum
//............................................................................

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
