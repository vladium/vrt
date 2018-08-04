
#include "vr/data/label_array.h"

#include "vr/strings.h"
#include "vr/util/logging.h"
#include "vr/mc/spinlock.h"
#include "vr/util/singleton.h"
#include "vr/util/str_range.h"

#include <algorithm>
#include <mutex>

//----------------------------------------------------------------------------
namespace vr
{
namespace data
{
//............................................................................
//............................................................................
namespace {
class label_array_factory; // forward
}
//............................................................................
//............................................................................

vr_static_assert (util::is_iterable<label_array>::value);
vr_static_assert (util::has_print<label_array>::value);

//............................................................................

template<>
struct label_array::access_by<label_array_factory>
{
    static void finalize_interning (label_array const & la)
    {
        la.finalize_interning ();
    }

}; // end of class
//............................................................................
//............................................................................
namespace
{
// note: addresses need to be stable

using label_array_ptr               = std::unique_ptr<label_array>;

struct label_array_ptr_equal: public std::binary_function<label_array_ptr, label_array_ptr, bool>
{
    bool operator() (label_array_ptr const & lhs, label_array_ptr const & rhs) const
    {
        assert_condition (lhs);
        assert_condition (rhs);

        return ((* lhs) == (* rhs));
    }

}; // end of functor

struct label_array_ptr_hasher: public std::unary_function<label_array_ptr, std::size_t>
{
    std::size_t operator() (label_array_ptr const & obj) const
    {
        assert_condition (obj);

        return hash_value (* obj);
    }

}; // end of functor

struct label_array_factory final: private noncopyable
{
    VR_FORCEINLINE label_array const & intern (label_array_ptr && la)
    {
        std::lock_guard<label_array_factory::lock_type> _ { m_lock };

        auto const rc = m_interned.insert (std::move (la)); // last use of 'la'

        DLOG_trace2 << "intern made actual insertion: " << rc.second << ", interned addr @" << rc.first->get ();

        if (rc.second)
        {
            label_array::access_by<label_array_factory>::finalize_interning (** rc.first);
        }

        return (** rc.first);
    }


    using lock_type                 = mc::spinlock<>;
    using intern_set                = boost::unordered_set<label_array_ptr, label_array_ptr_hasher, label_array_ptr_equal>;


    lock_type m_lock;
    intern_set m_interned;

}; // end of class

using label_array_factory_singleton = util::singleton<label_array_factory>;

} // end of 'anonymous'
//............................................................................
//............................................................................

int32_t
label_array::index (char_const_ptr_t const name, int32_t const name_size) const
{
    assert_nonzero (m_label_map.size ());

    util::str_range const name_sr { name, name_size };

    if (name_sr == NA_name ())
        return enum_NA ();

    auto const i = m_label_map.find (name_sr, util::str_range_hasher { }, util::str_range_equal { });
    if (VR_UNLIKELY (i == m_label_map.end ()))
        throw_x (invalid_input, "invalid label " + print (util::str_range { name, name_size }));

    return i->second;
}

int32_t
label_array::index (string_literal_t const name) const
{
    assert_nonzero (m_label_map.size ());

    if (name == NA_name ())
        return enum_NA ();

    auto const i = m_label_map.find (name);
    if (VR_UNLIKELY (i == m_label_map.end ()))
        throw_x (invalid_input, "invalid label " + print (name));

    return i->second;
}

void
label_array::finalize_interning () const
{
    check_zero (m_label_map.size ());

    for (int32_t i = 0, i_limit = size (); i != i_limit; ++ i)
    {
        m_label_map.emplace (m_labels [i].c_str (), i);
    }
}
//............................................................................

label_array const &
label_array::create_and_intern (string_vector const & labels)
{
    label_array_ptr la { new label_array { labels } }; // compute hash before acquiring any locks

    return label_array_factory_singleton::instance ().intern (std::move (la)); // last use of 'la'
}

label_array const &
label_array::create_and_intern (string_vector && labels)
{
    label_array_ptr la { new label_array { std::move (labels) } }; // compute hash before acquiring any locks

    return label_array_factory_singleton::instance ().intern (std::move (la)); // last use of 'la'
}
//............................................................................

inline std::size_t
label_array::compute_hash (string_vector const & labels)
{
    std::size_t h { };

    int32_t const i_limit = labels.size ();
    int32_t const i_limit_linear = std::min (9, i_limit); // TODO parameterize 9 threshold

    int32_t i;

    for (i = 0; i < i_limit_linear; ++ i)
    {
        util::hash_combine (h, str_hash_32 (labels [i]));
    }

    // switch to log sampling:

    for ( ; i < i_limit; i <<= 1)
    {
        util::hash_combine (h, str_hash_32 (labels [i]));
    }

    return h;
}

inline int32_t
label_array::check_labels (string_vector const & labels)
{
    check_nonempty (labels);

    using c_str_set     = boost::unordered_set<string_literal_t, util::c_str_hasher, util::c_str_equal>;

    int32_t r { };
    c_str_set uniq; // since 'labels' are pinned in memory, use a set of C-strings to avoid copying

    for (std::string const & l : labels)
    {
        check_nonempty (l);
        check_ne (l, NA_NAME_str); // disallow explicit NA specifications

        if (! uniq.insert (l.c_str ()).second)
            throw_x (invalid_input, "duplicate label " + print (l) + "");

        r = std::max<int32_t> (r, l.size ());
    }

    return r;
}
//............................................................................

std::string
__print__ (label_array const & obj) VR_NOEXCEPT
{
    std::stringstream s;
    s << obj.size () << " label(s) " << print (obj.m_labels);

    return s.str ();
}

} // end of 'data'
} // end of namespace
//----------------------------------------------------------------------------
