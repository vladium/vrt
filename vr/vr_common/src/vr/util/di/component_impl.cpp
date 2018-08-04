
#include "vr/util/di/component_impl.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
namespace di
{
//............................................................................

void
component_impl::dep_proxy::operator= (std::string const & name) const
{
    if (! m_ctx->m_dep_map.emplace (m_dep_field, std::make_tuple (name, m_bind_closure)).second)
        throw_x (invalid_input, "duplicate dep() bindings for the same field: " + print (name));
}

} // end of 'di'
} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
