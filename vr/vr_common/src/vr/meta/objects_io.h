#pragma once

#include "vr/meta/objects.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace meta
{

template<typename S>
struct print_visitor final
{
    print_visitor (S const & s, std::ostream & os) :
        m_s { s },
        m_os { os }
    {
    }

    template<typename FIELD_DEF>
    void operator() (FIELD_DEF)
    {
        using tag           = typename FIELD_DEF::tag;

        if (VR_UNLIKELY (! m_not_first))
            m_not_first = true;
        else
            m_os << ", ";

        meta::emit_tag_name<tag, /* QUOTE */false> (m_os);
        m_os << ": " << print (field<tag> (m_s));
    }

    S const & m_s;
    std::ostream & m_os;
    bool m_not_first { false };

}; // end of class


} // end of 'meta'
} // end of namespace
//----------------------------------------------------------------------------
