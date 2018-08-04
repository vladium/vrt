#pragma once

#include "vr/filesystem.h"
#include "vr/io/frame_streams.h"
#include "vr/util/memory.h"

#if ! defined (VR_USE_BLOSC) // TODO ASX-60
#   define VR_USE_BLOSC             0
#endif

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
//............................................................................
//............................................................................
namespace impl
{
/**
 * mostly used for debugging more complex frame stream impls
 */
class HDF_frame_istream_base: public frame_istream
{
    public: // ...............................................................

        // frame_istream:

        VR_ASSUME_COLD void close () final override; // idempotent

        VR_ASSUME_HOT int64_t read (data::dataframe & dst) final override;


        template<typename A> class access_by;

    protected: // ............................................................

        HDF_frame_istream_base (fs::path const & file, bitset32_t const mode, arg_map const & parms);

        HDF_frame_istream_base (HDF_frame_istream_base && rhs);

        ~HDF_frame_istream_base (); // calls 'close()'

    private: // ..............................................................

        template<typename A> friend class access_by;

        class state;

        /*
         * this reads HDF metadata to reconstruct an attribute schema
         *
         * post-condition: 'm_schema' has been set
         */
        VR_ASSUME_COLD VR_NOINLINE void initialize ();


        std::unique_ptr<state> m_state;

}; // end of class
//............................................................................
/**
 * mostly used for debugging more complex frame stream impls
 */
class HDF_frame_ostream_base: public frame_ostream
{
    public: // ...............................................................

        // frame_ostream:

        VR_ASSUME_COLD void close () final override; // idempotent

        VR_ASSUME_HOT int64_t write (data::dataframe const & src) final override;

    protected: // ............................................................

        HDF_frame_ostream_base (fs::path const & file, bitset32_t const mode, arg_map const & parms);

        HDF_frame_ostream_base (HDF_frame_ostream_base && rhs);

        ~HDF_frame_ostream_base (); // calls 'close()'

    private: // ..............................................................

        class state;

        /*
         * pre-condition: 'm_schema' has not been set
         */
        VR_ASSUME_COLD VR_NOINLINE void initialize (data::attr_schema::ptr const & schema);


        std::unique_ptr<state> m_state;

}; // end of class

} // end of 'impl'
//............................................................................
//............................................................................
/**
 * @note default 'EXC_POLICY' is "exceptions"
 */
template<typename EXC_POLICY = io::exceptions>
class HDF_frame_istream final: public impl::HDF_frame_istream_base
{
    private: // ..............................................................

        using super                 = impl::HDF_frame_istream_base;

    public: // ...............................................................

        HDF_frame_istream (arg_map const & parms, fs::path const & file, bitset32_t const mode = { }) :
            super (file, mode, parms)
        {
            // TODO EXC_POLICY
        }

}; // end of class
//............................................................................
/**
 * @note default 'EXC_POLICY' is "exceptions"
 */
template<typename EXC_POLICY = io::exceptions>
class HDF_frame_ostream final: public impl::HDF_frame_ostream_base
{
    private: // ..............................................................

        using super                 = impl::HDF_frame_ostream_base;

    public: // ...............................................................

        HDF_frame_ostream (arg_map const & parms, fs::path const & file, bitset32_t const mode =/* O_TRUNC, O_EXCL */{ }) :
            super (file, mode, parms)
        {
            // TODO EXC_POLICY
        }

}; // end of class

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
