#pragma once

#include "vr/data/attributes_fwd.h"
#include "vr/data/defs.h"
#include "vr/data/label_array.h"
#include "vr/enums.h"
#include "vr/meta/integer.h"
#include "vr/util/hashing.h" // c_str_hasher/c_str_equal
#include "vr/util/shared_object.h"

#include <boost/iterator/iterator_facade.hpp>

#include <algorithm>
#include <tuple>

//----------------------------------------------------------------------------
namespace vr
{
namespace data
{

class attr_type final: public boost::equality_comparable<attr_type> // hashable, copy/move-constructible, copy/move-assignable
{
    private: // ..............................................................

        using data_atype        = data::atype;

    public: // ..............................................................

        /*
         * not 'explicit' by design ('attribute' construction convenience)
         */
        attr_type (data::atype::enum_t const type) :
            m_data { reinterpret_cast<label_array const *> (type) }
        {
            check_ne (type, data_atype::category);
        }

        /*
         * not 'explicit' by design ('attribute' construction convenience)
         */
        attr_type (label_array const & labels) :
            m_data { & labels  } // note: this relies on the (statically asserted) fact that 'atype::category' is zero
        {
            assert_zero (static_cast<uint32_t> (reinterpret_cast<std_uintptr_t> (m_data)) & type_mask, m_data, labels);
        }

        template<typename E> // TODO enable only if 'E' is 'iterable' and 'printable'
        static typename std::enable_if<is_typesafe_enum_t<E>::value, attr_type>::type
        for_enum ();

        // ACCESSORs:

        data::atype::enum_t atype () const VR_NOEXCEPT
        {
            return static_cast<data_atype::enum_t> (static_cast<uint32_t> (reinterpret_cast<std_uintptr_t> (m_data)) & type_mask);
        }

        template<bool CHECK_TYPE = VR_CHECK_INPUT>
        label_array const & labels () const
        {
            // note: this relies on the (statically asserted) fact that 'atype::category' is zero

            label_array const * const r = m_data;

            if (CHECK_TYPE) // compile-time branch
            {
                auto const atid = this->atype ();
                if (VR_UNLIKELY (atid))
                    throw_x (type_mismatch, "attempt to get labels from an attribute of type " + print (atid));
            }

            return (* r);
        }

        // OPERATORs: defaults are fine

        // FRIENDs:

        friend bool operator== (attr_type const & lhs, attr_type const & rhs) VR_NOEXCEPT
        {
            return (lhs.m_data == rhs.m_data);
        }

        friend std::size_t hash_value (attr_type const & obj) VR_NOEXCEPT
        {
            return reinterpret_cast<std_uintptr_t> (obj.m_data); // TODO hash so that low bits don't cause poor distribution
        }

        friend std::ostream & operator<< (std::ostream & os, attr_type const & obj) VR_NOEXCEPT
        {
            return os << __print__ (obj);
        }

        friend VR_ASSUME_COLD std::string __print__ (attr_type const & obj) VR_NOEXCEPT;

    private: // ..............................................................

        static const int32_t type_mask      = ((1 << meta::static_log2_ceil<data_atype::size>::value) - 1);

        label_array const * /* const */ m_data; // low bits encode 'atype::enum_t'

}; // end of class
//............................................................................

template<typename E> // TODO enable only if 'E' is 'iterable' and 'printable'
typename std::enable_if<is_typesafe_enum_t<E>::value, attr_type>::type
attr_type::for_enum ()
{
    // TODO find an impl that preserves 'label_array' semantics but avoid temp label name listing

    string_vector labels;
    labels.reserve (E::size);

    for (typename E::enum_t e = E::first; e != E::size; ++ e)
    {
        labels.emplace_back (string_cast (e));
    }

    return { label_array::create_and_intern (std::move (labels)) }; // last use of'labels'
}
//............................................................................

class attribute final: public boost::equality_comparable<attribute> // hashable, copy/move-constructible, copy/move-assignable
{
    public: // ..............................................................

        attribute (std::string const & name, attr_type const & type) :
            m_type { type },
            m_name { name }
        {
        }

        attribute (std::string const & name, attr_type && type) :
            m_type { std::move (type) },
            m_name { name }
        {
        }

        attribute (string_literal_t const name, attr_type const & type) :
            m_type { type },
            m_name { name }
        {
        }

        attribute (string_literal_t const name, attr_type && type) :
            m_type { std::move (type) },
            m_name { name }
        {
        }

        // ACCESSORs:

        std::string const & name () const VR_NOEXCEPT
        {
            return m_name;
        }

        attr_type const & type () const VR_NOEXCEPT
        {
            return m_type;
        }

        /**
         * convenience shortcut, equivalent to <tt>type ().atype ()</tt>.
         */
        data::atype::enum_t atype () const VR_NOEXCEPT
        {
            return m_type.atype ();
        }

        /**
         * convenience shortcut, equivalent to <tt>type ().labels ()</tt>.
         */
        template<bool CHECK_TYPE = VR_CHECK_INPUT>
        label_array const & labels () const
        {
            return m_type.labels<CHECK_TYPE> ();
        }

        // OPERATORs: defaults are fine

        // static utilities:

        static std::vector<attribute> parse (std::string const & spec);

        // FRIENDs:

        friend bool operator== (attribute const & lhs, attribute const & rhs) VR_NOEXCEPT
        {
            if (& lhs == & rhs) return true;

            return (lhs.m_type == rhs.m_type) &&
                   (lhs.m_name == rhs.m_name);
        }

        friend std::size_t hash_value (attribute const & obj) VR_NOEXCEPT
        {
            std::size_t h { };

            util::hash_combine (h, obj.m_type);
            util::hash_combine (h, str_hash_32 (obj.m_name));

            return h;
        }

        friend std::ostream & operator<< (std::ostream & os, attribute const & obj) VR_NOEXCEPT
        {
            return os << __print__ (obj);
        }

        friend VR_ASSUME_COLD std::string __print__ (attribute const & obj) VR_NOEXCEPT;

    private: // ..............................................................

        friend class attr_schema; // grant access to the default constructor

        attribute () = default;


        attr_type /* const */ m_type { atype::size };
        std::string /* const */ m_name { };

}; // end of class
//............................................................................

class attr_schema final: public util::shared_object<attr_schema>,
                         public boost::equality_comparable<attr_schema> // hashable, move-constructible
{
    private: // ..............................................................

        template<typename C>
        struct is_non_schema_iterable: util::bool_constant<(util::is_iterable<C>::value && ! std::is_same<C, attr_schema>::value && ! util::is_std_string<C>::value)> { };

        class iterator_impl; // forward

    public: // ..............................................................

        using size_type     = width_type;

        /**
         * @param attrs any iterable 'attribute' container
         */
        template<typename C, typename _ = C>
        attr_schema (C const & attrs, typename std::enable_if<is_non_schema_iterable<_>::value>::type * = nullptr) :
            m_attrs { new attribute [attrs.size ()] }
        {
            std::copy (attrs.begin (), attrs.end (), m_attrs.get ());

            map_attr_names (m_attrs.get (), attrs.size (), m_attr_names);
        }

        /**
         * @param attrs any iterable 'attribute' container
         */
        template<typename C, typename _ = C>
        attr_schema (C && attrs, typename std::enable_if<(/* avoid stealing from lvalues: */std::is_rvalue_reference<decltype (attrs)>::value
                                 && is_non_schema_iterable<_>::value)>::type * = nullptr) :
            m_attrs { new attribute [attrs.size ()] }
        {
            auto const size = attrs.size (); // grab this before moving contents
            std::move (attrs.begin (), attrs.end (), m_attrs.get ()); // last reference to 'attrs'

            map_attr_names (m_attrs.get (), size, m_attr_names);
        }

        /**
         * equivalent to <tt>parse (spec)</tt>
         */
        attr_schema (std::string const & spec);

        /**
         * equivalent to <tt>parse (spec)</tt>
         */
        attr_schema (string_literal_t const spec);


        attr_schema (attr_schema && rhs) :
            m_attrs { std::move (rhs.m_attrs) },
            m_attr_names { std::move (rhs.m_attr_names) } // this is safe because 'rhs.m_attrs' gets moved
        {
            VR_IF_DEBUG (validate_attr_names (m_attrs.get (), m_attr_names);) // double-check the above
        }


        static attr_schema parse (const std::string & spec);

        // ACCESSORs:

        size_type size () const VR_NOEXCEPT
        {
            return m_attr_names.size ();
        }

        // position index lookup:

        template<bool CHECK_BOUNDS = VR_CHECK_INPUT>
        attribute const & at (size_type const index) const
        {
            if (CHECK_BOUNDS) check_within (index, size ());

            return m_attrs [index];
        }

        template<typename IX, typename _ = IX>
        typename std::enable_if<std::is_integral<_>::value, attribute const &>::type // disable ambiguity with other overloads
        operator[] (IX const index) const
        {
            return at<false> (index);
        }

        // name lookup:

        template<bool CHECK_NAME = VR_CHECK_INPUT>
        std::tuple<attribute const &, size_type> at (string_literal_t const name) const
        {
            auto const i = m_attr_names.find (name);

            if (CHECK_NAME && VR_UNLIKELY (i == m_attr_names.end ()))
                throw_x (invalid_input, "this schema has no attribute " + print (name));

            return std::forward_as_tuple (m_attrs [i->second], i->second);
        }

        template<bool CHECK_NAME = VR_CHECK_INPUT>
        std::tuple<attribute const &, size_type> at (std::string const & name) const
        {
            return at<CHECK_NAME> (name.c_str ());
        }

        std::tuple<attribute const &, size_type> operator[] (string_literal_t const name) const
        {
            return at<false> (name);
        }

        std::tuple<attribute const &, size_type> operator[] (std::string const & name) const
        {
            return at<false> (name);
        }


        // iteration:

        iterator_impl begin () const
        {
            return { * this, & m_attrs [0] };
        }

        iterator_impl end () const
        {
            return { * this, & m_attrs [0] + size () };
        }

        // FRIENDs:

        friend bool operator== (attr_schema const & lhs, attr_schema const & rhs) VR_NOEXCEPT
        {
            if (& lhs == & rhs) return true;

            auto const lhs_size = lhs.size ();

            if (lhs_size != rhs.size ())
                return false;

            for (size_type i = 0; i != lhs_size; ++ i)
            {
                if (! (lhs.m_attrs [i] == rhs.m_attrs [i]))
                    return false;
            }

            return true;
        }

        friend std::size_t hash_value (attr_schema const & obj) VR_NOEXCEPT
        {
            std::size_t h = obj.m_hash;
            if (h) return h;

            for (size_type i = 0, i_limit = obj.size (); i != i_limit; ++ i)
            {
                util::hash_combine (h, obj.m_attrs [i]);
            }

            // this lazy calc is MT-safe because it is computed from immutable content
            // and the actual mem store is atomic:

            return (obj.m_hash = h);
        }

        friend std::ostream & operator<< (std::ostream & os, attr_schema const & obj) VR_NOEXCEPT
        {
            return os << __print__ (obj);
        }

        friend VR_ASSUME_COLD std::string __print__ (attr_schema const & obj) VR_NOEXCEPT;

    private: // ..............................................................

        using attr_name_map     = boost::unordered_map<string_literal_t, size_type, util::c_str_hasher, util::c_str_equal>;


        struct iterator_impl final: public boost::iterator_facade<iterator_impl, attribute const, boost::forward_traversal_tag>
        {
            friend class boost::iterator_core_access;

            iterator_impl (attr_schema const & parent, attribute const * const position) VR_NOEXCEPT :
                m_parent { parent },
                m_position { position }
            {
            }

            // iterator_facade:

            void increment () VR_NOEXCEPT
            {
                ++ m_position;
            }

            const attribute & dereference () const VR_NOEXCEPT_IF (VR_RELEASE)
            {
                assert_within (m_position - & m_parent.m_attrs [0], m_parent.size ()); // attempt to dereference end/invalid iterator

                return (* m_position);
            }

            bool equal (iterator_impl const & rhs) const VR_NOEXCEPT
            {
                return (m_position == rhs.m_position);
            }


            attr_schema const & m_parent;
            attribute const * m_position;

        }; // end of nested class

        static void map_attr_names (attribute const * const attrs, size_type const size, attr_name_map & dst);
        VR_IF_DEBUG (static void validate_attr_names (attribute const * const attrs, attr_name_map const & map);)

        std::unique_ptr<attribute []> /* const */ m_attrs;
        mutable std::size_t m_hash { }; // transient
        attr_name_map /* const */ m_attr_names { }; // note: must be populated after 'm_attrs'

}; // end of class

} // end of 'data'
} // end of namespace
//----------------------------------------------------------------------------
