#pragma once

#include "vr/macros.h"
#include "vr/types.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
/**
 * TODO use the c++/ng version of the underlying lib to keep I/O consistent
 */
class getline_loop final: noncopyable
{
    public: // ...............................................................

        getline_loop (std::string const & prompt);
        getline_loop (string_literal_t const prompt);


        bool next (std::string & /* out */line);

    private: // ..............................................................

        std::string const m_prompt;

}; // end of class

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
