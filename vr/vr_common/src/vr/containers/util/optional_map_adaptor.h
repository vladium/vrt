#pragma once

#include "vr/exceptions.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
/**
 */
template<typename V>
class optional_map_adaptor final: public std::map<std::string, V>
{
    private: // ..............................................................

        using super     = std::map<std::string, V>;

    public: // ...............................................................

        using super::super; // inherit std::map constructors


        // ACCESSORs:

        V const & get (std::string const & name) const
        {
            auto const i = super::find (name);
            if (VR_UNLIKELY (i == super::end ()))
                throw_x (invalid_input, "no entry with name '" + name + '\'');

            return (i->second);
        }

        V get (std::string const & name, V const & dflt) const // note: value return type
        {
            auto const i = super::find (name);
            if (i == super::end ())
                return dflt;

            return (i->second);
        }

}; // end of class

} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
