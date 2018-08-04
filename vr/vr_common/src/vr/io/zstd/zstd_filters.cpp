
#include "vr/io/zstd/zstd_filters.h"

#include "vr/asserts.h"
#include "vr/util/logging.h"

#include <iostream>

#include <zstd.h>

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
//............................................................................
//............................................................................
namespace impl
{

VR_ENUM (stream_state,
    (
        created,
        open,
        flushing,
        closed
    )

); // end of enum
//............................................................................
// decompressor:
//............................................................................

struct zstd_decompressor_base::pimpl final
{
    using dstream_ptr       = std::unique_ptr<::ZSTD_DStream, std::size_t (*) (::ZSTD_DStream *)>;


    pimpl (arg_map const & parms)
    {
    }


    VR_ASSUME_COLD ::ZSTD_DStream * open ()
    {
        assert_condition (! m_dstream);

        ::ZSTD_DStream * ds;
        m_dstream.reset (ds = ::ZSTD_createDStream ());

        assert_nonnull (ds);

        auto const rc = ::ZSTD_initDStream (ds);
        if (VR_UNLIKELY (::ZSTD_isError (rc)))
            throw_x (io_exception, "zstd stream init failure: " + print (::ZSTD_getErrorName (rc)));

        m_state = stream_state::open;
        return ds;
    }

    VR_ASSUME_COLD void close ()
    {
        m_dstream.reset ();
        m_state = stream_state::closed;
    }


    dstream_ptr m_dstream { nullptr, ::ZSTD_freeDStream };
    stream_state::enum_t m_state { stream_state::created };

}; // end of class
//............................................................................
//............................................................................

zstd_decompressor_base::zstd_decompressor_base (arg_map const & parms) :
    m_impl { std::make_unique<pimpl> (parms) }
{
}

zstd_decompressor_base::~zstd_decompressor_base () = default;

//............................................................................

void
zstd_decompressor_base::close ()
{
    m_impl->close ();
}
//............................................................................

bool
zstd_decompressor_base::filter (char const * & begin_in, char const * const end_in, char * & begin_out, char * const end_out, bool const flush)
{
    std::size_t const n_in  { static_cast<std::size_t> (end_in - begin_in) };
    std::size_t const n_out { static_cast<std::size_t> (end_out - begin_out) };

    LOG_trace1 << "filter(" << static_cast<addr_const_t> (begin_in) << ", " << static_cast<addr_const_t> (end_in)
                            << ", " << static_cast<addr_const_t> (begin_out) << ", " << static_cast<addr_const_t> (end_out)
                            << ", flush: " << flush << "): entry (n_in: " << n_in << ", n_out: " << n_out << ')';

    bool r_flush { true }; // default to 'have more output'

    ::ZSTD_inBuffer in   { begin_in,  n_in,  0 };
    ::ZSTD_outBuffer out { begin_out, n_out, 0 };


    ::ZSTD_DStream * ds = m_impl->m_dstream.get ();

    switch (VR_LIKELY_VALUE (m_impl->m_state, stream_state::open))
    {
        case stream_state::created:
        {
            ds = m_impl->open (); // lazy open
            assert_eq (m_impl->m_state, stream_state::open);
        }
        /* no break */
        // !!! FALL THROUGH !!!


        case stream_state::open:
        {
            std::size_t rc { };

            while ((in.pos < n_in) && (out.pos < n_out)) // always write out as much as possible
            {
                rc = ::ZSTD_decompressStream (ds, & out, & in);
                if (VR_UNLIKELY (::ZSTD_isError (rc)))
                    throw_x (io_exception, "zstd stream decompression failure (rc " + string_cast (rc) + "): " + print (::ZSTD_getErrorName (rc)));

                // [assertion: 'rc' does not indicate zstd error]

                // 'rc' is a hint for next input count:

                assert_le (rc, ::ZSTD_DStreamInSize ()); // doc'ed guarantee

                DLOG_trace2 << "  dstream consumed total " << in.pos << '/' << n_in << " byte(s), transferred total " << out.pos << '/' << n_out << " byte(s), rc: " << rc;
            }

            r_flush = (rc > 0);
        }
        break;


        case stream_state::closed:
        {
            return false;
        }

    } // end of switch

    begin_in += in.pos;
    begin_out += out.pos;

    return r_flush;
}
//............................................................................

std::streamsize
zstd_decompressor_base::default_buffer_size ()
{
    std::size_t const zstd_out_size_hint = ::ZSTD_DStreamOutSize ();
    LOG_trace1 << "zstd output size hint: " << zstd_out_size_hint;

    return zstd_out_size_hint;
}
//............................................................................
// compressor:
//............................................................................

struct zstd_compressor_base::pimpl final
{
    using cstream_ptr       = std::unique_ptr<::ZSTD_CStream, std::size_t (*) (::ZSTD_CStream *)>;


    pimpl (arg_map const & parms) :
        m_compression_level { parms.get<int32_t> ("compression.level", default_compression_level ()) }
    {
        check_in_inclusive_range (m_compression_level, 1, ::ZSTD_maxCLevel ());

        LOG_trace1 << "using compression level " << m_compression_level;
    }


    VR_ASSUME_COLD ::ZSTD_CStream * open ()
    {
        assert_condition (! m_cstream);

        ::ZSTD_CStream * cs;
        m_cstream.reset (cs = ::ZSTD_createCStream ());

        assert_nonnull (cs);

        auto const rc = ::ZSTD_initCStream (cs, m_compression_level);
        if (VR_UNLIKELY (::ZSTD_isError (rc)))
            throw_x (io_exception, "zstd stream init failure: " + print (::ZSTD_getErrorName (rc)));

        m_state = stream_state::open;
        return cs;
    }

    VR_ASSUME_COLD void close ()
    {
        m_cstream.reset ();
        m_state = stream_state::closed; // TODO assert != flushing ?
    }


    cstream_ptr m_cstream { nullptr, ::ZSTD_freeCStream };
    int32_t const m_compression_level;
    stream_state::enum_t m_state { stream_state::created };

}; // end of class
//............................................................................
//............................................................................

zstd_compressor_base::zstd_compressor_base (arg_map const & parms) :
    m_impl { std::make_unique<pimpl> (parms) }
{
}

zstd_compressor_base::~zstd_compressor_base () = default;

//............................................................................

void
zstd_compressor_base::close ()
{
    m_impl->close ();
}
//............................................................................

bool
zstd_compressor_base::filter (char const * & begin_in, char const * const end_in, char * & begin_out, char * const end_out, bool const flush)
{
    std::size_t const n_in  { static_cast<std::size_t> (end_in - begin_in) };
    std::size_t const n_out { static_cast<std::size_t> (end_out - begin_out) };

    LOG_trace1 << "filter(" << static_cast<addr_const_t> (begin_in) << ", " << static_cast<addr_const_t> (end_in)
                            << ", " << static_cast<addr_const_t> (begin_out) << ", " << static_cast<addr_const_t> (end_out)
                            << ", flush: " << flush << "): entry (n_in: " << n_in << ", n_out: " << n_out << ')';

    bool r_flush { true }; // default to 'have more output'

    ::ZSTD_inBuffer in   { begin_in,  n_in,  0 };
    ::ZSTD_outBuffer out { begin_out, n_out, 0 };


    ::ZSTD_CStream * cs = m_impl->m_cstream.get ();

    switch (VR_LIKELY_VALUE (m_impl->m_state, stream_state::open))
    {
        case stream_state::created:
        {
            cs = m_impl->open (); // lazy open
            assert_eq (m_impl->m_state, stream_state::open);
        }
        /* no break */
        // !!! FALL THROUGH !!!


        case stream_state::open:
        {
            while ((in.pos < n_in) && (out.pos < n_out)) // always write out as much as possible
            {
                std::size_t const rc = ::ZSTD_compressStream (cs, & out, & in);
                if (VR_UNLIKELY (::ZSTD_isError (rc)))
                    throw_x (io_exception, "zstd stream compression failure (rc " + string_cast (rc) + "): " + print (::ZSTD_getErrorName (rc)));

                // [assertion: 'rc' does not indicate zstd error]

                // 'rc' is a hint for next input count:

                assert_le (rc, ::ZSTD_CStreamInSize ()); // doc'ed guarantee

                DLOG_trace2 << "  cstream transferred total " << in.pos << '/' << n_in << " byte(s), produced total " << out.pos << '/' << n_out << " byte(s), rc: " << rc;
            }

            if (VR_LIKELY (! flush || /* can flush only if have output space */(out.pos == n_out)))
                break;
            else // transition to 'flushing'
                m_impl->m_state = stream_state::flushing;
        }
        /* no break */
        // !!! FALL THROUGH !!!


        case stream_state::flushing:
        {
            assert_lt (out.pos, n_out);

            std::size_t const rc = ::ZSTD_endStream (cs, & out);
            if (VR_UNLIKELY (::ZSTD_isError (rc)))
                throw_x (io_exception, "zstd stream compression flush failure (rc " + string_cast (rc) + "): " + print (::ZSTD_getErrorName (rc)));

            // [assertion: 'rc' does not indicate zstd error]

            DLOG_trace2 << "  cstream produced total " << out.pos << '/' << n_out << " byte(s), remains to flush: " << rc;

            r_flush = rc;
        }
        break;

        case stream_state::closed:
        {
            return false;
        }

    } // end of switch

    begin_in += in.pos;
    begin_out += out.pos;

    return r_flush;
}
//............................................................................

std::streamsize
zstd_compressor_base::default_buffer_size ()
{
    std::size_t const zstd_in_size_hint = ::ZSTD_CStreamInSize ();
    LOG_trace1 << "zstd input size hint: " << zstd_in_size_hint;

    return zstd_in_size_hint;
}

} // end of 'impl'
//............................................................................

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
