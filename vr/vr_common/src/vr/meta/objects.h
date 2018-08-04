#pragma once

#include "vr/meta/utility.h"
#include "vr/util/type_traits.h"

#include <boost/mp11.hpp> // TODO narrow down include set
namespace bmp   = boost::mp11;

#include <utility> // std::integer_sequence

//----------------------------------------------------------------------------
namespace vr
{
namespace meta
{
//............................................................................

struct layout
{
    struct native   { };
    struct compact  { };
    struct packed   { };
    struct synth    { };

}; // end of scope
//............................................................................

template<typename S, typename = void>
struct is_meta_struct: std::false_type
{
}; // end of master

template<typename S>
struct is_meta_struct<S,
    util::void_t<typename S::_meta::metadata> >: std::true_type
{
}; // end of specialization
//............................................................................

template<typename S, typename = void>
struct fields_of
{
    using type      = bmp::mp_list<>;

}; // end of master

template<typename S>
struct fields_of<S,
    util::void_t<typename S::_meta::fields> >
{
    using type      = typename S::_meta::fields;

}; // end of specialization

template<typename S>
using fields_of_t           = typename fields_of<S>::type;

//............................................................................

template<typename S, typename = void>
struct metadata_of
{
    using type      = bmp::mp_list<>;

}; // end of master

template<typename S>
struct metadata_of<S,
    util::void_t<typename S::_meta::metadata> >
{
    using type      = typename S::_meta::metadata;

}; // end of specialization

template<typename S>
using metadata_of_t         = typename metadata_of<S>::type;

//............................................................................
//............................................................................
namespace impl
{
template<typename TAG, typename S>
struct field_offset_impl; // forward

//............................................................................
/*
 * 'true' iff 'T' is a class but one that's not part of a "typesafe enum" construct
 */
template<typename T>
using is_non_enum_class     =  util::bool_constant<(std::is_class<T>::value && ! is_typesafe_enum<T>::value)>;

/*
 * typesafe enum helper, local alias for convenience
 */
template<typename T>
using storage_t             = lower_if_typesafe_enum_t<T>;

//............................................................................

struct field_def_mark   { };
struct fdef_mark        { };
struct field_desc_mark  { };
struct elide_mark       { };

} // end of 'impl'
//............................................................................
//............................................................................

template<typename T, typename TAG>
struct field_def: public impl::field_def_mark
{
    using value_type                = T;
    using tag                       = TAG;

}; // end of class

template<bool ENABLED = true> // note: parameter provided to support uniform mark use in templated code
struct elide: public impl::elide_mark
{
    static constexpr bool enabled ()    { return ENABLED; }

}; // end of class
//............................................................................
//............................................................................
namespace impl // resume 'impl'
{

using elide_default     = elide<false>;

//............................................................................
/*
 * non-variadic normalized form of 'fdef_<>'
 */
template<typename /* field_def<> */FIELD_DEF, bool ELIDE>
struct field_desc: public field_desc_mark // base used only for static [concept] checking
{
    using field_def_type                    = FIELD_DEF;
    static constexpr bool elide ()          { return ELIDE; }

    using field_value_type                  = typename FIELD_DEF::value_type;
    using field_tag                         = typename FIELD_DEF::tag;

}; // end of class

template<typename /* field_def<> */FIELD_DEF, bool ELIDE, int32_t DECL_INDEX>
struct field_desc_and_index final: public field_desc<FIELD_DEF, ELIDE>
{
    using super                             = field_desc<FIELD_DEF, ELIDE>;
    static constexpr int32_t decl_index ()  { return DECL_INDEX; }

}; // end of class


template<typename /* field_desc[_and_index]<> */FIELD_DESC>
using get_field_def_t   = typename FIELD_DESC::field_def_type;

template<typename /* field_desc[_and_index]<> */FIELD_DESC>
struct elided_predicate
{
     static constexpr bool value    = FIELD_DESC::elide ();

}; // end of predicate
//............................................................................

template<typename /* fdef_<> */FDEF, bool HAS_FIELD_DEF = false>
struct fdef_to_field_def
{
    using type          = field_def<bmp::mp_first<FDEF>, bmp::mp_second<FDEF>>; // caller asserts correct arity

}; // end of master

template<typename /* fdef_<> */FDEF>
struct fdef_to_field_def<FDEF, /* HAS_FIELD_DEF */true>
{
    using type          = typename FDEF::field_def_parm;

}; // end of specialization
//............................................................................

template<typename /* fdef_<> */FDEF>
struct make_field_desc
{
    static_assert (std::is_base_of<fdef_mark, FDEF>::value, "'FIELD' must be 'field<...>'");

    using field_def_type    = typename fdef_to_field_def<FDEF, FDEF::has_field_def ()>::type;
    using type              = field_desc<field_def_type, FDEF::elide ()>;

}; // end of mp metafunction

template<typename /* fdef_<> */FDEF>
using make_field_desc_t     = typename make_field_desc<FDEF>::type;


template<typename /* field_desc<> */FIELD_DESC, typename FIELD_DECL_INDEX>
struct make_field_desc_and_index
{
    static_assert (std::is_base_of<field_desc_mark, FIELD_DESC>::value, "'FIELD_DESC' must be 'field_desc<...>'");

    using field_def_type    = typename FIELD_DESC::field_def_type;
    using type              = field_desc_and_index<field_def_type, FIELD_DESC::elide (), FIELD_DECL_INDEX::value>;

}; // end of mp metafunction

template<typename /* field_desc<> */FIELD_DESC, typename FIELD_DECL_INDEX>
using make_field_desc_and_index_t   = typename make_field_desc_and_index<FIELD_DESC, FIELD_DECL_INDEX>::type;

template<typename /* field_desc_and_index<> */FIELD_DESC_AND_INDEX>
using as_field_desc_t       = typename FIELD_DESC_AND_INDEX::super;

//............................................................................
/*
 * normalize a sequence of field defs (fdef_<...>) into a sequence of field descriptors (field_desc<...>)
 */
template<typename FIELD_DEF_seq>
struct make_schema_impl
{
    static_assert (bmp::mp_is_list<FIELD_DEF_seq>::value, "'FIELD_DEF_seq' must be an mp11 list");

    using type              = bmp::mp_transform<make_field_desc_t, FIELD_DEF_seq>;

}; // end of metafunction
//............................................................................

template<typename FIELD_DESC, typename TAG_set>
struct materialize_field_desc_contained_in // used by 'select_fields_impl'
{
    using type          = field_desc<typename FIELD_DESC::field_def_type, (! bmp::mp_set_contains<TAG_set, typename FIELD_DESC::field_tag>::value)>;

}; // end of predicate

/*
 * keep only those 'FIELD_DESC_seq' schema fields whose tags appear in 'TAG_seq'
 */
template<typename FIELD_DESC_seq, typename TAG_seq>
struct select_fields_impl
{
    static_assert (bmp::mp_is_list<FIELD_DESC_seq>::value, "'FIELD_DESC_seq' must be an mp11 list");
    static_assert (bmp::mp_is_set<TAG_seq>::value, "'TAG_seq' must be an mp11 set");

    template<typename FIELD_DESC>
    using materialize   = typename materialize_field_desc_contained_in<FIELD_DESC, TAG_seq>::type;

    using type          = bmp::mp_transform<materialize, FIELD_DESC_seq>;

}; // end of metafunction
//............................................................................
/*
 * extend 'FIELD_DESC_seq' schema with field defs (fdef_<...>) in 'FIELD_DEF_seq'
 */
template<typename FIELD_DESC_seq, typename FIELD_DEF_seq>
struct extend_schema_impl
{
    static_assert (bmp::mp_is_list<FIELD_DESC_seq>::value, "'FIELD_DESC_seq' must be an mp11 list");
    static_assert (bmp::mp_is_list<FIELD_DEF_seq>::value, "'FIELD_DEF_seq' must be an mp11 list");

    using type          = bmp::mp_append<FIELD_DESC_seq, typename make_schema_impl<FIELD_DEF_seq>::type>;

}; // end of metafunction
//............................................................................

template<typename T, typename TAG, typename = void>
struct aligned_vholder; // master


template<typename T, typename TAG> // 'T' is a (non-typesafe enum) class
struct aligned_vholder<T, TAG,
    util::enable_if_t<is_non_enum_class<T>::value>>: public T // inherit from 'T'
{
    using value_type    = T;

    using T::T; // inherit constructors

    struct access final
    {
        template<typename S>
        static VR_FORCEINLINE value_type const & r_value (S const & s) VR_NOEXCEPT
        {
            return static_cast<aligned_vholder const &> (s);
        }

        template<typename S>
        static VR_FORCEINLINE value_type & l_value (S & s) VR_NOEXCEPT
        {
            return static_cast<aligned_vholder &> (s);
        }

    }; // end of nested scope

}; // end of specialization

template<typename T, typename TAG> // 'T' is not inheritable from
struct aligned_vholder<T, TAG,
    util::enable_if_t<(! is_non_enum_class<T>::value)>>
{
    using value_type    = T;
    using storage_type  = storage_t<T>;

//    template<typename ... ARGs> // allow non-default 'T' construction
//    aligned_vholder (ARGs && ... args) :
//        m_field { std::forward<ARGs> (args) ... }
//    {
//    }

    struct access final
    {
        template<typename S>
        static VR_FORCEINLINE storage_type const & r_value (S const & s) VR_NOEXCEPT
        {
            return static_cast<aligned_vholder const &> (s).m_field;
        }

        template<typename S>
        static VR_FORCEINLINE storage_type & l_value (S & s) VR_NOEXCEPT
        {
            return static_cast<aligned_vholder &> (s).m_field;
        }

    }; // end of nested scope

    storage_type m_field; // aggregate 'T'

}; // end of specialization
//............................................................................

template<typename T, typename TAG>
struct packed_vholder
{
    using value_type    = T;
    using storage_type  = storage_t<T>;

    static constexpr int32_t field_size     = storage_size_of<storage_type>::value;
    using field_storage = padding<field_size>;


    struct access final
    {
        template<typename S>
        static VR_FORCEINLINE storage_type const & r_value (S const & s) VR_NOEXCEPT
        {
            return reinterpret_cast<storage_type const &> (static_cast<packed_vholder const &> (s)._);
        }

        template<typename S>
        static VR_FORCEINLINE storage_type & l_value (S & s) VR_NOEXCEPT
        {
            return reinterpret_cast<storage_type &> (static_cast<packed_vholder &> (s)._);
        }

    }; // end of nested scope

    field_storage _;

}; // end of specialization
//............................................................................

template<typename T, typename S, typename AT>
struct l_value_proxy final // used by 'synth_vholder::access::l_value'
{
    VR_FORCEINLINE l_value_proxy (S & s) :
        m_s { s }
    {
    }


    VR_FORCEINLINE operator T () const
    {
        return AT::get ()(m_s);
    }

    VR_FORCEINLINE void operator= (T const rhs)
    {
        AT::set ()(m_s, rhs);
    }

    S & m_s;

}; // end of nested class


template<typename AT, typename TAG> // does not need partial specialization for primitive/class 'T'
struct synth_vholder // empty class
{

    struct access final
    {
        template<typename S>
        static VR_FORCEINLINE auto r_value (S const & s)
        {
            return AT::get ()(s);
        }

        template<typename S>
        static VR_FORCEINLINE auto l_value (S & s) -> l_value_proxy<storage_t<decltype (r_value (s))>, S, AT>
        {
            return { s };
        }

    }; // end of nested scope

}; // end of class
//............................................................................

template<typename T, typename TAG> // does not need partial specialization for primitive/class 'T'
struct elided_vholder // empty class
{
    using value_type    = T;
    using storage_type  = storage_t<T>;

    template<typename ... ARGs> // allow non-default no-op construction
    elided_vholder (ARGs && ... args)
    {
    }

    struct access final
    {
        template<typename S>
        static VR_NORETURN storage_type const & r_value (S const & s)
        {
            VR_ASSUME_UNREACHABLE (cn_<TAG> (), cn_<S> ());
        }

        template<typename S>
        static VR_NORETURN storage_type & l_value (S & s)
        {
            VR_ASSUME_UNREACHABLE (cn_<TAG> (), cn_<S> ());
        }

    }; // end of nested scope

}; // end of class
//............................................................................

template<typename VHOLDER, typename S>
struct offset_within_impl
{
    static_assert (std::is_base_of<VHOLDER, S>::value, "TODO");

    static constexpr int32_t calculate ()
    {
        constexpr const S * s { nullptr };

        return (static_cast<int8_t const *> (static_cast<void const *> (& static_cast<VHOLDER const &> (* s)))
                - static_cast<int8_t const *> (static_cast<void const *> (s)));
    }
};

template<typename /* field_desc<> */FIELD_DESC, template<typename, typename, typename ...> class VHOLDER>
struct ftraits
{
    using fdesc         = FIELD_DESC;
    using vholder       = VHOLDER<typename FIELD_DESC::field_value_type, typename FIELD_DESC::field_tag>;

    private: template<typename, typename> friend class field_offset_impl; // grant access to 'offset_within()'

        template<typename S>
        static constexpr int32_t offset_within ()   { return offset_within_impl<vholder, S>::calculate (); }

}; // end of traits

template<typename /* ftraits<> */FTRAITS>
using get_vholder_t     = typename FTRAITS::vholder;

//............................................................................

template<typename LAYOUT>
struct select_vholder; // master

template<>
struct select_vholder<layout::compact>
{
    template<typename T, typename TAG>
    using type          = aligned_vholder<T, TAG>;

}; // end of specialization

template<>
struct select_vholder<layout::packed>
{
    template<typename T, typename TAG>
    using type          = packed_vholder<T, TAG>;

}; // end of specialization

template<>
struct select_vholder<layout::synth>
{
    template<typename AT, typename TAG>
    using type          = synth_vholder<AT, TAG>;

}; // end of specialization
//............................................................................

template<typename LAYOUT>
struct ftrait_ops final
{
    template<typename T, typename TAG>
    using non_elided_vholder    = typename select_vholder<LAYOUT>::template type<T, TAG>;

    // TODO  for 'synth' this still leaves 'AT' as 'T' in 'FIELD_DESC':

    template<typename FIELD_DESC>
    using make_ftraits_t    = util::if_t<FIELD_DESC::elide (), ftraits<FIELD_DESC, elided_vholder>, ftraits<FIELD_DESC, non_elided_vholder>>;

}; // end of class

template<typename /* ftraits<> */FTRAITS>
using as_metadata_entry_t   = std::pair<typename FTRAITS::fdesc::field_tag, FTRAITS>;

//............................................................................

template<typename ... BASEs>
struct merge_meta_info
{
    using meta_bases    = bmp::mp_copy_if<bmp::mp_list<BASEs ...>, is_meta_struct>;

    using fields_seq    = bmp::mp_transform<fields_of_t, meta_bases>;
    using metadata_seq  = bmp::mp_transform<metadata_of_t, meta_bases>;

    // return types:

    using fields        = bmp::mp_apply<bmp::mp_append, fields_seq>;
    using metadata      = bmp::mp_apply<bmp::mp_append, metadata_seq>;

}; // end of metafunction
//............................................................................

template<typename /* field_desc[_and_index]<> */FIELD_DESC>
struct field_storage_size
{
    static_assert (std::is_base_of<field_desc_mark, FIELD_DESC>::value, "'FIELD_DESC' must be 'field_desc<...>'");

    static constexpr std::size_t storage_size   = storage_size_of<typename FIELD_DESC::field_value_type>::value;
    static constexpr std::size_t value          = (FIELD_DESC::elide () ? 0 : storage_size);

}; // end of metafunction

template<typename FIELD_DESC_AND_INDEX_LHS, typename FIELD_DESC_AND_INDEX_RHS>
struct field_desc_compact_predicate
{
    static_assert (std::is_base_of<field_desc_mark, FIELD_DESC_AND_INDEX_LHS>::value, "'FIELD_DESC_AND_INDEX_LHS' must be 'field_desc_and_index<...>'");
    static_assert (std::is_base_of<field_desc_mark, FIELD_DESC_AND_INDEX_RHS>::value, "'FIELD_DESC_AND_INDEX_RHS' must be 'field_desc_and_index<...>'");

    static constexpr std::size_t lhs_storage_size   = field_storage_size<FIELD_DESC_AND_INDEX_LHS>::value;
    static constexpr std::size_t rhs_storage_size   = field_storage_size<FIELD_DESC_AND_INDEX_RHS>::value;

    static constexpr bool value     = (lhs_storage_size > rhs_storage_size)
                                      ||
                                      ((lhs_storage_size == rhs_storage_size) && (FIELD_DESC_AND_INDEX_LHS::decl_index () < FIELD_DESC_AND_INDEX_RHS::decl_index ()));

}; // end of class

template<typename FIELD_DESC_seq>
struct compact_reorder_schema
{
    // add indexes to 'FIELD_DESC_seq':

    using indices                   = bmp::mp_iota<bmp::mp_size<FIELD_DESC_seq>>;
    using indexed_field_desc_seq    = bmp::mp_transform<make_field_desc_and_index_t, FIELD_DESC_seq, indices>;

    // re-order (index values ensure stable sorting):

    using compact_field_desc_seq    = bmp::mp_sort<indexed_field_desc_seq, field_desc_compact_predicate>;

    // remove indexes:

    using type                      = bmp::mp_transform<as_field_desc_t, compact_field_desc_seq>;

}; // end of metafunction
//............................................................................

template<typename LAYOUT, typename FIELD_DESC_seq, typename ... BASEs>
struct make_struct final
{
    static_assert (bmp::mp_is_list<FIELD_DESC_seq>::value, "'FIELD_SPEC_seq' must be an mp11 list");

    // metadata (contains elided and base fields):

    template<typename FIELD_DESC>
    using ftrait_constructor        = typename ftrait_ops<LAYOUT>::template make_ftraits_t<FIELD_DESC>;

    using field_desc_laid_out_seq   = util::if_t
                                        <
                                            std::is_same<LAYOUT, layout::packed>::value,
                                            FIELD_DESC_seq,
                                            typename compact_reorder_schema<FIELD_DESC_seq>::type
                                        >;

    using ftrait_seq                = bmp::mp_transform<ftrait_constructor, field_desc_laid_out_seq>;
    using field_holder_seq          = bmp::mp_transform<get_vholder_t, ftrait_seq>; // used by 'mp_inherit' below

    using metadata_map              = bmp::mp_transform<as_metadata_entry_t, ftrait_seq>;

    using bases_info                = merge_meta_info<BASEs ...>; // combine 'BASEs' linearly

    using all_metadata      = bmp::mp_append<typename bases_info::metadata, metadata_map>;
    static_assert (bmp::mp_is_map<all_metadata>::value, "bad struct metadata (duplicate field tags?)");

    // fields (contains base but no elided fields):

    using non_elided_fields         = bmp::mp_remove_if<FIELD_DESC_seq, elided_predicate>;
    using decl_order_map            = bmp::mp_transform<get_field_def_t, non_elided_fields>; // is also a valid 'bmp' map
    using all_fields        = bmp::mp_append<typename bases_info::fields, decl_order_map>;

    // finally, the struct type:

    struct type: public BASEs ..., public bmp::mp_apply<bmp::mp_inherit, field_holder_seq>
    {
        struct _meta final // used as a meta-struct marker, among other things
        {
            using metadata      = all_metadata; // [includes elided fields] in memory layout order
            using fields        = all_fields;   // [excludes elided fields] in source decl order

        }; // end of nested scope
    };

}; // end of metafunction
//............................................................................

template<typename FIELD_DESC_seq>
struct synthetic_meta final
{
    static_assert (bmp::mp_is_list<FIELD_DESC_seq>::value, "'FIELD_SPEC_seq' must be an mp11 list");

    // metadata (contains elided fields):

    template<typename FIELD_DESC>
    using ftrait_constructor        = typename ftrait_ops<layout::synth>::template make_ftraits_t<FIELD_DESC>;

    using ftrait_seq                = bmp::mp_transform<ftrait_constructor, FIELD_DESC_seq>;
    using metadata_map              = bmp::mp_transform<as_metadata_entry_t, ftrait_seq>;

    // fields (contains no elided fields):

    using non_elided_fields         = bmp::mp_remove_if<FIELD_DESC_seq, elided_predicate>;

    // finally, the '_meta' definition:

    struct type final
    {
        using metadata      = metadata_map;         // [includes elided fields] in 'FIELD_DESC_seq' order
        using fields        = non_elided_fields;    // [excludes elided fields] in 'FIELD_DESC_seq' order

    }; // end of nested scope

}; // end of metafunction
//............................................................................

struct dummy_value_type final
{
    template<typename T>
    VR_NORETURN operator T const & () const
    {
        VR_ASSUME_UNREACHABLE (cn_<T> ());
    }

    template<typename T>
    VR_NORETURN operator T & ()
    {
        VR_ASSUME_UNREACHABLE (cn_<T> ());
    }


    template<typename T>
    VR_NORETURN T & operator= (T &&)
    {
        VR_ASSUME_UNREACHABLE (cn_<T> ());
    }

    friend std::ostream & operator<< (std::ostream & os, dummy_value_type const & obj)
    {
        VR_ASSUME_UNREACHABLE (cn_<dummy_value_type> ());
    }

}; // end of class

struct dummy_vholder final
{
    using value_type    = dummy_value_type;

    struct access final
    {
        template<typename S>
        static VR_NORETURN value_type const & r_value (S const & s)
        {
            VR_ASSUME_UNREACHABLE ();
        }

        template<typename S>
        static VR_NORETURN value_type & l_value (S & s)
        {
            VR_ASSUME_UNREACHABLE ();
        }

    }; // end of nested scope

}; // end of class

template<typename TAG>
struct dummy_ftraits // c.f. 'ftraits'
{
    using fdesc         = field_desc<field_def<dummy_value_type, TAG>, /* ELIDE */true>;
    using vholder       = dummy_vholder;

    private: template<typename, typename> friend class field_offset_impl; // grant access to 'offset_within()'

        template<typename S>
        static constexpr int32_t offset_within ()   { return 0; }

}; // end of traits

template<typename MD_ENTRY, typename TAG>
struct md_entry_to_ftraits
{
    using type          = bmp::mp_second<MD_ENTRY>;

}; // end of master

template<typename TAG>
struct md_entry_to_ftraits<void, TAG>
{
    using type          = dummy_ftraits<TAG>;

}; // end of specialization
//............................................................................
/*
 * note: this supports cases when 'S:
 *
 *  1. has 'TAG' field but it's elided (no special handling needed here);
 *  2. does not have a 'TAG' field;
 *  3. is not a meta object at all.
 */
template<typename TAG, typename S>
struct find_ftraits
{
    using md_entry          = bmp::mp_map_find<metadata_of_t<S>, TAG>; // note: 'mn_entry' will be 'void' in cases 2 and 3
    using type              = typename md_entry_to_ftraits<md_entry, TAG>::type;

}; // end of metafunction

template<typename TAG, typename S>
using find_ftraits_t    = typename find_ftraits<TAG, S>::type;

//............................................................................

template<typename TAG, typename S>
using find_vholder_t    = typename find_ftraits_t<TAG, S>::vholder;

//............................................................................

template<typename MD_ENTRY>
struct has_tagged_field_impl
{
    static constexpr bool value = (! bmp::mp_second<MD_ENTRY>::fdesc::elide ());

}; // end of metafunction master

template<>
struct has_tagged_field_impl<void>
{
    static constexpr bool value = false;

}; // end of metafunction specialization
//............................................................................

template<typename TAG, typename S>
struct field_offset_impl
{
    static constexpr int32_t value  = find_ftraits_t<TAG, S>::template offset_within<S> ();

}; // end of metafunction

} // end of 'impl'
//............................................................................
//............................................................................
/**
 * allowed forms:
 *
 *  fdef_<T, TAG[, elide<...>]>
 *  fdef_<field_def<T, TAG>[, elide<...>]>
 */
template<typename ... Ts> // using a variadic form intentionally, for terser demangled names
struct fdef_ final: public impl::fdef_mark
{
    static constexpr int32_t arity  = sizeof ... (Ts);

    using field_def_parm                        = util::find_derived_t<impl::field_def_mark, void, Ts ...>;
    static constexpr bool has_field_def ()      { return (! util::is_void<field_def_parm>::value); }

    static_assert ((2 <= (arity + has_field_def ()) && (arity + has_field_def ()) <= 3),
       "valid syntax is fdef_<T, TAG[, elide<...>]> and fdef_<field_def<T, TAG>[, elide<...>]>");

    static constexpr bool elide ()              { return util::find_derived_t<impl::elide_mark, impl::elide_default, Ts ...>::enabled (); }

}; // end of class
//............................................................................

template<typename ... FIELD_DEFs>
using make_schema_t         = typename impl::make_schema_impl<bmp::mp_list<FIELD_DEFs ...>>::type;

template<typename FIELD_DESC_seq, typename ... FIELD_DEFs>
using extend_schema_t       = typename impl::extend_schema_impl<FIELD_DESC_seq, bmp::mp_list<FIELD_DEFs ...>>::type;

//............................................................................
/**
 * @note: fields that are not selected are elided (and not removed from the schema)
 */
template<typename FIELD_DESC_seq, typename ... TAGs>
using select_fields_t       = typename impl::select_fields_impl<FIELD_DESC_seq, bmp::mp_list<TAGs ...>>::type;

template<typename ... FIELD_DEFs>
using synthetic_meta_t      = typename impl::synthetic_meta<make_schema_t<FIELD_DEFs ...>>::type;

//............................................................................

template<typename LAYOUT, typename FIELD_DESC_seq, typename ... BASEs>
using make_struct_t         = typename impl::make_struct<LAYOUT, FIELD_DESC_seq, BASEs ...>::type;

template<typename FIELD_DESC_seq, typename ... BASEs>
using make_compact_struct_t = make_struct_t<layout::compact, FIELD_DESC_seq, BASEs ...>;

template<typename FIELD_DESC_seq, typename ... BASEs> // TODO param to generate default init
using make_packed_struct_t  = make_struct_t<layout::packed, FIELD_DESC_seq, BASEs ...>;

//............................................................................

template<typename TAG, typename S>
constexpr bool
has_field ()
{
    return impl::has_tagged_field_impl<bmp::mp_map_find<metadata_of_t<S>, TAG>>::value;
}

#define obj_has_field(TAG, s)   (meta::has_field< TAG, std::decay_t<decltype (s)>> ())

//............................................................................
/**
 * returns 'field_def<...>' for field 'TAG' of 'S'
 *
 * note: presense of 'TAG' in 'S' is not asserted
 */
template<typename TAG, typename S>
struct find_field_def
{
    using type              = typename impl::find_ftraits_t<TAG, S>::fdesc::field_def_type;

}; // end of metafunction

template<typename TAG, typename S>
using find_field_def_t      = typename find_field_def<TAG, S>::type;

//............................................................................

template<typename TAG, typename S>
constexpr int32_t
field_offset ()
{
    return impl::field_offset_impl<TAG, S>::value;
};
//............................................................................

template<typename TAG, typename S> // TODO combine this with the non-const overload? (e.g., tag dispatch)
VR_FORCEINLINE auto
field (S const & s) -> decltype (impl::find_vholder_t<TAG, S>::access::r_value (s))
{
    using vholder       = impl::find_vholder_t<TAG, S>;

    return vholder::access::r_value (s);
}

template<typename TAG, typename S>
VR_FORCEINLINE auto
field (S & s) -> decltype (impl::find_vholder_t<TAG, S>::access::l_value (s))
{
    using vholder       = impl::find_vholder_t<TAG, S>;

    return vholder::access::l_value (s);
}
//............................................................................
/**
 * a utility struct alias
 */
using empty_struct      = make_packed_struct_t<make_schema_t<>>;

//............................................................................

template<typename S, typename V>
void
apply_visitor (V && f)
{
    using S_fields      = typename S::_meta::fields;

    bmp::mp_for_each<S_fields> (std::forward<V> (f));
}

} // end of 'meta'
} // end of namespace
//----------------------------------------------------------------------------
