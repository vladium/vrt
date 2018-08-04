#pragma once

#include "vr/any.h"
#include "vr/types.h"

#include <map>

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
/**
 */
class any_map: public std::map<std::string, any>
{
    private: // ..............................................................

        using super     = std::map<std::string, any>;

    public: // ...............................................................

        using super::super; // inherit std::map constructors

        // ACCESSORs:

        template<typename T>
        auto
        get (std::string const & name) const -> decltype (any_get<T> (any { }))
        {
            auto const i = find (name);
            if (VR_UNLIKELY (i == end ()))
                throw_x (invalid_input, "no entry with name '" + name + '\'');

            return any_get<T> (i->second); // will throw on 'T' mismatch
        }

        template<typename T>
        lower_if_typesafe_enum_t<T> // note: returning by value
        get (std::string const & name, lower_if_typesafe_enum_t<T> const & dflt) const // overloaded to avoid returning a ref to 'dflt'
        {
            auto const i = find (name);
            if (i == end ())
                return dflt;

            return any_get<T> (i->second); // will throw on 'T' mismatch
        }

}; // end of class

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
