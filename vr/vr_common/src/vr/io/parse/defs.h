#pragma once

#include "vr/enums.h"
#include "vr/util/str_range.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
namespace parse
{
/*
 * TODO a 'token_and_position' subclass might be handy
 */
struct CSV_token final // note: not inheriting from 'util::str_range' to optimize for size
{
    VR_NESTED_ENUM (
        (
            NA_token,
            quoted_name,
            num_int,
            num_fp,
            datetime
        ),
        printable

    ) // end of enum

    CSV_token (string_literal_t const start, int32_t const size, enum_t const type) :
        m_start { start },
        m_size { size },
        m_type { type }
    {
    }

    // ACCESSORs:

    util::str_range str_range () const
    {
        return { m_start, m_size };
    }

    friend VR_ASSUME_COLD std::string __print__ (CSV_token const & obj) VR_NOEXCEPT;


    string_literal_t const m_start;
    int32_t const m_size;
    enum_t const m_type;

}; // end of class
//............................................................................

vr_static_assert (sizeof (CSV_token) == 16);

//............................................................................
/*
 *
 */
struct JSON_token final // note: not inheriting from 'util::str_range' to optimize for size
{
    VR_NESTED_ENUM (
        (
            null,
            string,
            boolean,
            num_int,
            num_fp,

            eoa // this is a bit of a HACK to avoid using a proper parser to handle vector fields
        ),
        printable

    ) // end of enum

    JSON_token (string_literal_t const start, int32_t const size, enum_t const type) :
        m_start { start },
        m_size { size },
        m_type { type }
    {
    }

    // ACCESSORs:

    util::str_range str_range () const
    {
        return { m_start, m_size };
    }

    friend VR_ASSUME_COLD std::string __print__ (JSON_token const & obj) VR_NOEXCEPT;


    string_literal_t const m_start;
    int32_t const m_size;
    enum_t const m_type;

}; // end of class
//............................................................................

vr_static_assert (sizeof (JSON_token) == 16);

//............................................................................

} // end of 'parse'
} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
