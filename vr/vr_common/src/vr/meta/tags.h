#pragma once

#include "vr/util/classes_fwd.h" // util::class_name<>

#include <boost/preprocessor/cat.hpp>

//----------------------------------------------------------------------------
namespace vr
{
namespace meta
{
//............................................................................

#define vr_META_tag_for(name)  BOOST_PP_CAT (_, BOOST_PP_CAT (name, _))

//............................................................................

#define VR_META_TAG(name)                   \
    struct vr_META_tag_for(name)    { }     \
    /* */

//............................................................................

extern void
emit_tag_class_name_as_field_name (std::string && tag_cls_name, bool const quote, std::ostream & os);

//............................................................................

template<typename TAG, bool QUOTE = false>
void
emit_tag_name (std::ostream & os)
{
    emit_tag_class_name_as_field_name (util::class_name<TAG> (), QUOTE, os);
}

} // end of 'meta'
} // end of namespace
//----------------------------------------------------------------------------
