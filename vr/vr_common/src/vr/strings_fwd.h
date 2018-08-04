#pragma once

#include "vr/types.h"

//----------------------------------------------------------------------------
namespace vr
{
//............................................................................

template<char QUOTE = '"'>
std::string
quote_string (string_literal_t const s);

template<char QUOTE = '"'>
std::string
quote_string (std::string const & s);

//............................................................................

template<typename T>
std::string
print (T const & x); // forward

template<std::size_t N>
std::string
print (char const (& s) [N]); // forward

//............................................................................

} // end of namespace
//----------------------------------------------------------------------------
