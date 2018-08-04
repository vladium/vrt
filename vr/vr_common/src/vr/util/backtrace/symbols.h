#pragma once

#include "vr/exceptions.h" // trace_data

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
namespace backtrace
{
//............................................................................

extern std::string
demangle (string_literal_t const mangled_name);

//............................................................................

// TODO multi-trace overload, for caching lookups b/w similar traces

extern void
print_trace (trace_data const & trace, std::ostream & out, std::string const & prefix = { });

//............................................................................

} // end of 'backtrace'
} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
