#pragma once

#include "vr/io/frame_streams.h"
#include "vr/io/parse/defs.h" // CSV_token
#include "vr/io/streams.h"

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
class CSV_frame_istream_base: public frame_istream
{
    public: // ...............................................................

        // frame_istream:

        VR_ASSUME_COLD void close () final override; // idempotent

        VR_ASSUME_HOT int64_t read (data::dataframe & dst) final override;

    protected: // ............................................................

        CSV_frame_istream_base (arg_map const & parms, std::unique_ptr<std::istream> && is);

        CSV_frame_istream_base (CSV_frame_istream_base && rhs) = default;

        ~CSV_frame_istream_base () // calls 'close()'
        {
            close ();
        }


        std::istream & stream ();

    private: // ..............................................................

        /*
         * pre-condition: 'm_schema' has not been set
         */
        VR_ASSUME_COLD VR_NOINLINE void initialize (data::attr_schema::ptr const & schema);


        std::unique_ptr<std::istream> m_is;
        std::string m_line { };
        std::vector<parse::CSV_token> m_tokens;
        int32_t const m_header_row_count;
        /*char const m_separator;*/

}; // end of class
//............................................................................
/**
 * mostly used for debugging more complex frame stream impls
 */
class CSV_frame_ostream_base: public frame_ostream
{
    public: // ...............................................................

        // frame_ostream:

        VR_ASSUME_COLD void close () final override; // idempotent

        VR_ASSUME_HOT int64_t write (data::dataframe const & src) final override;

    protected: // ............................................................

        // TODO take a ptime format parm?
        CSV_frame_ostream_base (arg_map const & parms, std::unique_ptr<std::ostream> && os);

        CSV_frame_ostream_base (CSV_frame_ostream_base && rhs) = default;

        ~CSV_frame_ostream_base () // calls 'close()'
        {
            close ();
        }


        std::ostream & stream ();

    private: // ..............................................................

        /*
         * pre-condition: 'm_schema' has not been set
         */
        VR_ASSUME_COLD VR_NOINLINE void initialize (data::attr_schema::ptr const & schema);


        std::unique_ptr<std::ostream> m_os;
        int32_t const m_digits; //
        int32_t const m_digits_secs; // [0, 9]
        char const m_separator;

}; // end of class

} // end of 'impl'
//............................................................................
//............................................................................
/**
 * @note default 'EXC_POLICY' is "exceptions"
 */
template<typename IS, typename EXC_POLICY = io::exceptions>
class CSV_frame_istream final: public impl::CSV_frame_istream_base
{
    private: // ..............................................................

        using super                 = impl::CSV_frame_istream_base;

    public: // ...............................................................

        template<typename ... ARGs>
        CSV_frame_istream (arg_map const & parms, ARGs && ... args) :
            super (parms, std::make_unique<IS> (std::forward<ARGs> (args) ...))
        {
            setup_istream (stream (), EXC_POLICY { });
        }

}; // end of class
//............................................................................
/**
 * @note default 'EXC_POLICY' is "exceptions"
 */
template<typename OS, typename EXC_POLICY = io::exceptions>
class CSV_frame_ostream final: public impl::CSV_frame_ostream_base
{
    private: // ..............................................................

        using super                 = impl::CSV_frame_ostream_base;

    public: // ...............................................................

        template<typename ... ARGs>
        CSV_frame_ostream (arg_map const & parms, ARGs && ... args) :
            super (parms, std::make_unique<OS> (std::forward<ARGs> (args) ...))
        {
            setup_ostream (stream (), EXC_POLICY { });
        }

}; // end of class


} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
