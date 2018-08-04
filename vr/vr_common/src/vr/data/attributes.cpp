
#include "vr/data/attributes.h"

#include "vr/data/parse/attr_parser.h"
#include "vr/strings.h"
#include "vr/util/logging.h"
#include "vr/util/memory.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace data
{
//............................................................................

vr_static_assert (sizeof (attr_type) == sizeof (addr_t)); // tight
vr_static_assert (util::has_print<attr_type>::value);

vr_static_assert (sizeof (attribute) == sizeof (attr_type) + sizeof (std::string)); // tight
vr_static_assert (util::has_print<attribute>::value);

vr_static_assert (util::is_iterable<attr_schema>::value);
vr_static_assert (util::has_print<attr_schema>::value);

//............................................................................

std::vector<attribute>
attribute::parse (std::string const & spec)
{
    return parse::attr_parser::parse (spec);
}
//............................................................................

void
attr_schema::map_attr_names (attribute const * const attrs, size_type const size, attr_name_map & dst)
{
    check_nonnegative (size);

    for (size_type i = 0; i != size; ++ i)
    {
        DLOG_trace1 << '[' << i << "] mapping " << attrs [i];

        if (VR_UNLIKELY (! dst.emplace (attrs [i].name ().c_str (), i).second))
            throw_x (invalid_input, "duplicate attribute name " + print (attrs [i].name ()));
    }
}
//............................................................................
#if VR_DEBUG

void
attr_schema::validate_attr_names (attribute const * const attrs, attr_name_map const & map)
{
    for (auto const & ai : map)
    {
        check_eq (static_cast<addr_const_t> (ai.first), static_cast<addr_const_t> (attrs [ai.second].name ().c_str ()), ai.second);
    }
}

#endif // VR_DEBUG
//............................................................................

attr_schema::attr_schema (std::string const & spec) :
    attr_schema (attribute::parse (spec))
{
}

attr_schema::attr_schema (string_literal_t const spec) :
    attr_schema (std::string { spec })
{
}
//............................................................................

attr_schema
attr_schema::parse (const std::string & spec)
{
    return { attribute::parse (spec) };
}
//............................................................................

std::string
__print__ (attr_type const & obj) VR_NOEXCEPT
{
    std::stringstream s;

    s << print (obj.atype ());

    if (obj.atype () == atype::category)
    {
        s << ", " << print (obj.labels ());
    }

    return s.str ();
}

std::string
__print__ (attribute const & obj) VR_NOEXCEPT
{
    std::stringstream s;

    s << print (obj.name ())  << ": " << print (obj.type ());

    return s.str ();
}

std::string
__print__ (attr_schema const & obj) VR_NOEXCEPT
{
    std::stringstream s;

    s << obj.size () << " attribute(s):\n{\n";

    for (auto const & a : obj)
    {
        s << "  " << print (a) << ";\n";
    }

    s << '}';

    return s.str ();
}

} // end of 'data'
} // end of namespace
//----------------------------------------------------------------------------
