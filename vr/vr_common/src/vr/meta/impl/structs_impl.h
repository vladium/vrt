#pragma once

#include "vr/meta/objects_io.h" // meta::print_visitor
#include "vr/preprocessor.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace meta
{
//............................................................................

#define vr_META_PACKED_STRUCT_2(cls, field_seq)                                     \
    vr_META_PACKED_STRUCT_PROLOGUE_2 (cls, VR_DOUBLE_PARENTHESIZE_2 (field_seq))    \
    vr_META_STRUCT_BODY (cls, VR_DOUBLE_PARENTHESIZE_2 (field_seq))                 \
    vr_META_STRUCT_EPILOGUE ()                                                      \
    /* */

#define vr_META_PACKED_STRUCT_3(cls, base, field_seq)                                   \
    vr_META_PACKED_STRUCT_PROLOGUE_3 (cls, base, VR_DOUBLE_PARENTHESIZE_2 (field_seq))  \
    vr_META_STRUCT_BODY (cls, VR_DOUBLE_PARENTHESIZE_2 (field_seq))                     \
    vr_META_STRUCT_EPILOGUE ()                                                          \
    /* */


#define vr_META_PACKED_STRUCT_PROLOGUE_2(cls, field_seq) \
    namespace impl_struct \
    { \
        using BOOST_PP_CAT (fields_, cls) = meta::make_schema_t<vr_META_field_decl_list (field_seq) >; \
        using BOOST_PP_CAT (base_, cls) = meta::make_packed_struct_t< BOOST_PP_CAT (fields_, cls) >; \
    } \
    struct cls : public impl_struct:: BOOST_PP_CAT (base_, cls) \
    { \
    /* */

#define vr_META_PACKED_STRUCT_PROLOGUE_3(cls, base, field_seq) \
    namespace impl_struct \
    { \
        using BOOST_PP_CAT (fields_, cls) = meta::make_schema_t< vr_META_field_decl_list (field_seq) >; \
        using BOOST_PP_CAT (base_, cls) = meta::make_packed_struct_t< BOOST_PP_CAT (fields_, cls) , base >; \
    } \
    struct cls : public impl_struct:: BOOST_PP_CAT (base_, cls) \
    { \
    /* */

//............................................................................

#define vr_META_COMPACT_STRUCT_2(cls, field_seq)                                    \
    vr_META_COMPACT_STRUCT_PROLOGUE_2 (cls, VR_DOUBLE_PARENTHESIZE_2 (field_seq))   \
    vr_META_STRUCT_BODY (cls, VR_DOUBLE_PARENTHESIZE_2 (field_seq))                 \
    vr_META_STRUCT_EPILOGUE ()                                                      \
    /* */

#define vr_META_COMPACT_STRUCT_3(cls, base, field_seq)                                  \
    vr_META_COMPACT_STRUCT_PROLOGUE_3 (cls, base, VR_DOUBLE_PARENTHESIZE_2 (field_seq)) \
    vr_META_STRUCT_BODY (cls, VR_DOUBLE_PARENTHESIZE_2 (field_seq))                     \
    vr_META_STRUCT_EPILOGUE ()                                                          \
    /* */


#define vr_META_COMPACT_STRUCT_PROLOGUE_2(cls, field_seq) \
    namespace impl_struct \
    { \
        using BOOST_PP_CAT (fields_, cls) = meta::make_schema_t< vr_META_field_decl_list (field_seq) >; \
        using BOOST_PP_CAT (base_, cls) = meta::make_compact_struct_t< BOOST_PP_CAT (fields_, cls) >; \
    } \
    struct cls : public impl_struct:: BOOST_PP_CAT (base_, cls) \
    { \
    /* */

#define vr_META_COMPACT_STRUCT_PROLOGUE_3(cls, base, field_seq) \
    namespace impl_struct \
    { \
        using BOOST_PP_CAT (fields_, cls) = meta::make_schema_t< vr_META_field_decl_list (field_seq) >; \
        using BOOST_PP_CAT (base_, cls) = meta::make_compact_struct_t< BOOST_PP_CAT (fields_, cls) , base >; \
    } \
    struct cls : public impl_struct:: BOOST_PP_CAT (base_, cls) \
    { \
    /* */

//............................................................................

#define vr_META_STRUCT_EPILOGUE() \
    } /* user provides final semicolon */ \
    /* */

//............................................................................

#define vr_META_STRUCT_BODY(cls, field_seq) \
    vr_META_STRUCT_field_accessor_list (field_seq) \
    vr_META_STRUCT_printable(cls, field_seq) \
    vr_META_STRUCT_streamable(cls) \
    /* */

//............................................................................
/*
 * type -> BOOST_PP_TUPLE_ELEM (2, 0, type_name_tup)
 * name -> BOOST_PP_TUPLE_ELEM (2, 1, type_name_tup)
 *
 * tag  -> vr_META_tag_for (name)
 */
//............................................................................

#define vr_META_field_decl_list(field_seq) \
    BOOST_PP_SEQ_FOR_EACH_I (vr_META_field_decl, unused, field_seq) \
    /* */

#define vr_META_field_decl(r, data, i, type_name_tup) \
    BOOST_PP_COMMA_IF (i) \
    meta::fdef_< BOOST_PP_TUPLE_ELEM (2, 0, type_name_tup), vr_META_tag_for (BOOST_PP_TUPLE_ELEM (2, 1, type_name_tup)) > \
    /* */

//............................................................................

#define vr_META_STRUCT_field_accessor_list(field_seq) \
    BOOST_PP_SEQ_FOR_EACH (vr_META_field_accessor, unused, field_seq) \
    /* */

#define vr_META_field_accessor(r, data, type_name_tup) \
    meta::impl::storage_t< BOOST_PP_TUPLE_ELEM (2, 0, type_name_tup) > const & BOOST_PP_TUPLE_ELEM (2, 1, type_name_tup) () const \
    { \
        return meta::field< vr_META_tag_for (BOOST_PP_TUPLE_ELEM (2, 1, type_name_tup)) > (* this); \
    } \
    meta::impl::storage_t< BOOST_PP_TUPLE_ELEM (2, 0, type_name_tup) > & BOOST_PP_TUPLE_ELEM (2, 1, type_name_tup) () \
    { \
        return meta::field< vr_META_tag_for (BOOST_PP_TUPLE_ELEM (2, 1, type_name_tup)) > (* this); \
    } \
    /* */

//............................................................................

#define vr_META_STRUCT_printable(cls, field_seq) \
    friend std::string __print__ ( cls const & obj) VR_NOEXCEPT \
    { \
        std::stringstream os { }; \
        os << '{'; \
        { \
            meta::print_visitor< cls > v { obj, os }; \
            meta::apply_visitor< cls > (v); \
        } \
        os << '}'; \
        return os.str (); \
    } \
    /* */

//............................................................................

#define vr_META_STRUCT_streamable(cls) \
    friend std::ostream & operator<< (std::ostream & os, cls const & obj) VR_NOEXCEPT \
    { \
        return os << __print__ (obj); \
    } \

//............................................................................

#define vr_META_tag_for(name)  BOOST_PP_CAT (_, BOOST_PP_CAT (name, _))

//............................................................................

} // end of 'meta'
} // end of namespace
//----------------------------------------------------------------------------
