#pragma once

#include "vr/filesystem.h"
#include "vr/util/hashing.h"
#include "vr/util/str_range.h"
#include "vr/util/type_traits.h"

#include <boost/operators.hpp>
#include <boost/regex.hpp>

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
//............................................................................
//............................................................................
namespace impl
{

struct components_base
{
        components_base (components_base && rhs) = default;

    protected:

        components_base (boost::smatch && sm);

        components_base (components_base const & rhs)               = delete;
        components_base & operator= (components_base const & rhs)   = delete;


        boost::smatch m_sm;

}; // end of nested class

} // end of 'impl'
//............................................................................
//............................................................................
/**
 * @see stream_factory
 */
class uri final: public boost::totally_ordered<uri> // hashable, copy/move-constructible
{
    public: // ...............................................................

        struct components final: impl::components_base
        {
            util::str_range scheme () const;
            util::str_range authority () const;
            util::str_range path () const;
            util::str_range query () const;
            util::str_range fragment () const;

            private:

                friend class uri;

                using impl::components_base::components_base;

        }; // end of nested class


        uri (string_literal_t const s);
        uri (std::string const & s);
        uri (std::string && s);

        // convenience constructors:

        uri (fs::path const & file); // creates a "file://" uri using 'weak_canonical_path(file)'

        // ACCESSORs:

        components split () const; // note: this returns a view

        std::string const & native () const
        {
            return m_native;
        }

        // OPERATORs:

        friend bool operator< (uri const & lhs, uri const & rhs) VR_NOEXCEPT
        {
            return (lhs.m_native < rhs.m_native); // not dealing with case sensitivity, etc
        }

        friend bool operator== (uri const & lhs, uri const & rhs) VR_NOEXCEPT
        {
            return (lhs.m_native == rhs.m_native); // not dealing with case sensitivity, etc
        }

        // FRIENDs:

        friend std::size_t hash_value (uri const & obj) VR_NOEXCEPT
        {
            return util::hash<std::string> { }(obj.m_native);
        }

        friend VR_ASSUME_COLD std::string __print__ (uri const & obj) VR_NOEXCEPT
        {
            return '[' + obj.m_native + ']';
        }

        friend std::ostream & operator<< (std::ostream & os, uri const & obj) VR_NOEXCEPT
        {
            return os << obj.m_native;
        }

    private: // ..............................................................

        void validate () const;

        std::string const m_native;

}; // end of class

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
