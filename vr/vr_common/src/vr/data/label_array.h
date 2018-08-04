#pragma once

#include "vr/data/NA.h"
#include "vr/util/hashing.h" // c_str_hasher/c_str_equal

#include <boost/operators.hpp>

//----------------------------------------------------------------------------
namespace vr
{
namespace data
{
/**
 */
class label_array final: public boost::equality_comparable<label_array>, // hashable
                         noncopyable
{
    public: // ...............................................................

        static label_array const & create_and_intern (string_vector const & labels);
        static label_array const & create_and_intern (string_vector && labels);

        // ACCESSORs:

        int32_t size () const
        {
            return m_labels.size ();
        }

        template<bool CHECK_BOUNDS = VR_CHECK_INPUT>
        std::string const & at (int32_t const index) const
        {
            if (CHECK_BOUNDS) check_in_range (index, -1, size ());

            return (index < 0 ? NA_NAME_str : m_labels [index]);
        }

        std::string const & operator[] (int32_t const index) const
        {
            assert_in_range (index, -1, size ());

            return at<false> (index);
        }

        // map labels to label indices (available only on interned instances):

        /**
         * @param name [non necessarily 0-terminated]
         */
        VR_ASSUME_HOT int32_t index (char_const_ptr_t const name, int32_t const name_size) const;

        /**
         * @param name [0-terminated]
         */
        VR_ASSUME_HOT int32_t index (string_literal_t const name) const;

        VR_FORCEINLINE int32_t index (std::string const & name) const
        {
            return index (name.data (), name.size ());
        }

        // iteration:

        string_vector::const_iterator begin () const
        {
            return m_labels.begin ();
        }

        string_vector::const_iterator end () const
        {
            return m_labels.end ();
        }

        // format assists:

        int32_t const & max_label_size () const
        {
            return m_max_label_size;
        }

        // OPERATORs:

        friend inline bool operator== (label_array const & lhs, label_array const & rhs) VR_NOEXCEPT
        {
            if (& lhs == & rhs) return true;

            return (lhs.m_hash == rhs.m_hash) &&
                   (lhs.m_labels == rhs.m_labels);
        }

        friend inline std::size_t hash_value (label_array const & obj) VR_NOEXCEPT
        {
            return obj.m_hash;
        }

        friend std::ostream & operator<< (std::ostream & os, label_array const & obj) VR_NOEXCEPT
        {
            return os << __print__ (obj);
        }

        friend VR_ASSUME_COLD std::string __print__ (label_array const & obj) VR_NOEXCEPT;

        template<typename A> class access_by;

    private: // ..............................................................

        template<typename A> friend class access_by;

        using label_map     = boost::unordered_map<string_literal_t, int32_t, util::c_str_hasher, util::c_str_equal>;


        label_array (string_vector const & labels) :
            m_labels (labels), // note: vector construction
            m_hash { compute_hash (m_labels) },
            m_max_label_size { check_labels (m_labels) } // non-empty, unique, etc
        {
        }

        label_array (string_vector && labels) :
            m_labels (std::move (labels)), // note: vector construction
            m_hash { compute_hash (m_labels) },
            m_max_label_size { check_labels (m_labels) } // non-empty, unique, etc
        {
        }


        void finalize_interning () const; // invoked on successful interning, only once

        static VR_ASSUME_HOT std::size_t compute_hash (string_vector const & labels);
        static VR_ASSUME_HOT int32_t check_labels (string_vector const & labels);


        string_vector const m_labels;
        std::size_t const m_hash;
        label_map mutable m_label_map;
        int32_t const m_max_label_size; // set by 'check_labels()'

}; // end of class

} // end of 'data'
} // end of namespace
//----------------------------------------------------------------------------
