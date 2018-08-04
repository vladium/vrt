#pragma once

#include "vr/exceptions.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
//............................................................................

/*
 * a "fast" exception
 */
VR_DEFINE_EXCEPTION (stop_iteration,        control_flow_exception);

//............................................................................

VR_DEFINE_EXCEPTION (operation_redundant,   io_exception);
VR_DEFINE_EXCEPTION (operation_not_allowed, io_exception);

//............................................................................

class http_exception: public io_exception
{
    public: // ...............................................................

        using io_exception::io_exception; // inherit constructors

        explicit VR_FORCEINLINE http_exception (VR_EXC_LOC_ARGS, std::string const & msg, int32_t const status) VR_NOEXCEPT :
            io_exception (file, line, func, msg),
            m_status { status }
        {
        }

        // TODO:

//        VR_NESTED_ENUM (
//            (
//                BOOST_PP_SEQ_ENUM (VR_IO_HTTP_STATUS_SEQ)
//            ),
//            printable
//
//        ) // end of enum

        /**
         * @return HTTP status code [zero if not available]
         */
        int32_t const & status () const
        {
            return m_status;
        }

    protected: // ............................................................

        int32_t const m_status;

}; // end of class

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
