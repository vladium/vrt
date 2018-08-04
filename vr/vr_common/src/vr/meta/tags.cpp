
#include "vr/meta/tags.h"

#include "vr/asserts.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace meta
{
//............................................................................

void
emit_tag_class_name_as_field_name (std::string && tag_cls_name, bool const quote, std::ostream & os)
{
    auto i_limit = tag_cls_name.size ();
    assert_positive (i_limit);

    auto i = tag_cls_name.find_last_of (':');
    assert_ne (i, std::string::npos);
    assert_lt (i, i_limit - 1);

    if (VR_LIKELY ('_' == tag_cls_name [i + 1])) // assume '...::_NAME_' convention
    {
        i += 2,
        -- i_limit;
    }
    else // assume '...::NAME' convention
    {
        ++ i;
    }

    if (quote) os.put ('"'); // TODO can also override 'tag_cls_name' in place in some cases
    os.write (& tag_cls_name [i], i_limit - i);
    if (quote) os.put ('"');
}

} // end of 'meta'
} // end of namespace
//----------------------------------------------------------------------------
