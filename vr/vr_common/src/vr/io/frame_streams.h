#pragma once

#include "vr/arg_map.h"
#include "vr/data/attributes.h"
#include "vr/filesystem.h"

#include <fcntl.h>

//----------------------------------------------------------------------------
namespace vr
{
namespace data {
class dataframe; // forward
}

namespace io
{

class frame_stream: noncopyable // move-constructible TODO
{
    public: // ...............................................................

        frame_stream (frame_stream && rhs) :
            m_schema { rhs.m_schema },
            m_last_io_df { rhs.m_last_io_df }
        {
        }

        virtual ~frame_stream () = default;

        // ACCESSORs:

        data::attr_schema::ptr const schema () const
        {
            check_condition (m_schema); // may be available after construction or after some I/O has been done

            return m_schema;
        }

        // MUTATORs:

        virtual void close () = 0;

        // TRAITs:

        struct traits final
        {

        };

    protected: // ............................................................

        frame_stream () = default;


        data::attr_schema::ptr m_schema { };
        data::dataframe const * m_last_io_df { }; // optimization assist

    private: // ..............................................................

}; // end of class
//............................................................................
/**
 */
class frame_istream: public frame_stream
{
    public: // ...............................................................

        static std::unique_ptr<frame_istream> open (fs::path const & file, arg_map const & parms);

        // MUTATORs:

        virtual int64_t read (data::dataframe & dst) = 0;

    protected: // ............................................................

        frame_istream () = default;

}; // end of class
//............................................................................
/**
 */
class frame_ostream: public frame_stream
{
    public: // ...............................................................

        static std::unique_ptr<frame_ostream> create (fs::path const & file, arg_map const & parms, fs::path * const out_file = nullptr);

        // MUTATORs:

        virtual int64_t write (data::dataframe const & src) = 0;

    protected: // ............................................................

        frame_ostream () = default;

}; // end of class
//............................................................................

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
