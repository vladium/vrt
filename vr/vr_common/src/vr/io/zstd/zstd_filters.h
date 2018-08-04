#pragma once

#include "vr/arg_map.h"

#include <boost/iostreams/filter/symmetric.hpp>

#include <memory>

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
//............................................................................
//............................................................................
namespace impl
{
//............................................................................

struct zstd_filter_base
{
    using char_type     = char;

}; // end of class
//............................................................................

class zstd_decompressor_base: public zstd_filter_base,
                              noncopyable
{
    public: // ...............................................................

        static std::streamsize default_buffer_size ();

        zstd_decompressor_base (arg_map const & parms);
        ~zstd_decompressor_base ();


        // boost::iostreams::SymmetricFilter:

        VR_ASSUME_COLD void close ();

        VR_ASSUME_HOT bool filter (char const * & begin_in, char const * const end_in, char * & begin_out, char * const end_out, bool const flush);

    private: // ..............................................................

        class pimpl;

        std::unique_ptr<pimpl> const m_impl;

}; // end of class

class zstd_compressor_base: public zstd_filter_base,
                            noncopyable
{
    public: // ...............................................................

        static constexpr int32_t default_compression_level ()   { return 3; } // ZSTD_CLEVEL_DEFAULT, which doesn't seem to be exposed via <zstd.h>
        static std::streamsize default_buffer_size ();

        zstd_compressor_base (arg_map const & parms);
        ~zstd_compressor_base ();


        // boost::iostreams::SymmetricFilter:

        VR_ASSUME_COLD void close ();

        VR_ASSUME_HOT bool filter (char const * & begin_in, char const * const end_in, char * & begin_out, char * const end_out, bool const flush);

    private: // ..............................................................

        class pimpl;

        std::unique_ptr<pimpl> const m_impl;

}; // end of class
//............................................................................

template<typename ALLOC = std::allocator<zstd_filter_base::char_type> >
class zstd_decompressor_impl: public zstd_decompressor_base
{
    private: // ..............................................................

        using super         = zstd_decompressor_base;

    public: // ...............................................................

        using super::super;

}; // end of class

template<typename ALLOC = std::allocator<zstd_filter_base::char_type> >
class zstd_compressor_impl: public zstd_compressor_base
{
    private: // ..............................................................

        using super         = zstd_compressor_base;

    public: // ...............................................................

        using super::super;

}; // end of class

} // end of 'impl'
//............................................................................
//............................................................................

template<typename ALLOC = std::allocator<typename impl::zstd_filter_base::char_type> >
class zstd_decompressor: public boost::iostreams::symmetric_filter<impl::zstd_decompressor_impl<ALLOC>, ALLOC>
{
    private: // ..............................................................

        using impl_type     = impl::zstd_decompressor_impl<ALLOC>;
        using super         = boost::iostreams::symmetric_filter<impl_type, ALLOC>;

    public: // ...............................................................

        using char_type     = typename super::char_type;

        struct category: super::category,
                         boost::iostreams::optimally_buffered_tag
        { };

        static VR_ASSUME_COLD std::streamsize optimal_buffer_size () { return impl_type::default_buffer_size (); }


        zstd_decompressor (arg_map const & parms = { }, std::streamsize const buffer_size = optimal_buffer_size ()) :
            super (buffer_size, parms)
        {
        }

}; // end of class

template<typename ALLOC = std::allocator<typename impl::zstd_filter_base::char_type> >
class zstd_compressor: public boost::iostreams::symmetric_filter<impl::zstd_compressor_impl<ALLOC>, ALLOC>
{
    private: // ..............................................................

        using impl_type     = impl::zstd_compressor_impl<ALLOC>;
        using super         = boost::iostreams::symmetric_filter<impl_type, ALLOC>;

    public: // ...............................................................

        using char_type     = typename super::char_type;

        struct category: super::category,
                         boost::iostreams::optimally_buffered_tag
        { };

        static VR_ASSUME_COLD std::streamsize optimal_buffer_size () { return impl_type::default_buffer_size (); }


        zstd_compressor (arg_map const & parms = { }, std::streamsize const buffer_size = optimal_buffer_size ()) :
            super (buffer_size, parms)
        {
        }

}; // end of class

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
