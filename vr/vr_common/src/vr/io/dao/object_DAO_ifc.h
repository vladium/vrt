#pragma once

#include "vr/fields.h"
#include "vr/io/json/json_object_serializer.h"
#include "vr/io/sql/sqlite/sqlite_connection.h"
#include "vr/io/sql/tx_guard.h"
#include "vr/util/datetime.h"
#include "vr/utility.h" // optional

struct sqlite3_stmt; // forward

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
//............................................................................
//............................................................................
namespace impl
{
//............................................................................

extern void
bind (::sqlite3_stmt * const stmt, string_literal_t const name, int32_t const value);

extern void
bind (::sqlite3_stmt * const stmt, string_literal_t const name, std::string const & value);

//............................................................................

template<typename ID_VALUE_TYPE, typename = void>
struct bind_ID; // master

template<typename ID_VALUE_TYPE> // specialize for 32-bit ints
struct bind_ID<ID_VALUE_TYPE,
    util::enable_if_t<std::is_integral<ID_VALUE_TYPE>::value && (sizeof (ID_VALUE_TYPE) == 4)>>
{
    static VR_FORCEINLINE void evaluate (::sqlite3_stmt * const stmt, ID_VALUE_TYPE const & ID, string_literal_t const ID_name)
    {
        bind (stmt, ID_name, ID);
    }

    static string_literal_t data_type ()
    {
        return "INTEGER";
    }

}; // end of specialization

template<typename ID_VALUE_TYPE> // specialize for 'std::string'
struct bind_ID<ID_VALUE_TYPE,
    util::enable_if_t<std::is_same<ID_VALUE_TYPE, std::string>::value>>
{
    static VR_FORCEINLINE void evaluate (::sqlite3_stmt * const stmt, std::string const & ID, string_literal_t const ID_name)
    {
        bind (stmt, ID_name, ID);
    }

    static string_literal_t data_type ()
    {
        return "TEXT";
    }

}; // end of specialization
//............................................................................

template<typename T, typename ID_FIELD, typename ID_VALUE_TYPE, typename = void> // allow for specialization if becomes necessary
struct bind_obj_ID
{
    static VR_FORCEINLINE void evaluate (::sqlite3_stmt * const stmt, T const & obj, string_literal_t const ID_name)
    {
        vr_static_assert (std::is_same<T, std::decay_t<T>>::value); // 'T' is already bare

        bind (stmt, ID_name, field<ID_FIELD> (obj)); // will pick one of the overloads
    }

}; // end of master
//............................................................................

template<typename T>
void
unmarshall (util::str_range const & s, optional<T> & obj_optional)
{
    T obj { };
    json_object_serializer::unmarshall (s, obj);

    obj_optional = std::move (obj); // note: last use of 'obj'
}
//............................................................................

using bind_ID_fn        = void (*) (::sqlite3_stmt * const stmt, addr_const_t/* ID_value_type */ const ID, string_literal_t const ID_name);
using bind_obj_ID_fn    = void (*) (::sqlite3_stmt * const stmt, addr_const_t/* T */ const obj, string_literal_t const ID_name);

using unmarshall_fn     = void (*) (util::str_range const & s, addr_t/* T */ const obj);
using marshall_fn       = std::ostream & (*) (addr_const_t/* T */ const obj, std::ostream &);

struct type_erased_fns final
{
    string_literal_t const m_key_data_type;
    bind_ID_fn const m_bifn;
    bind_obj_ID_fn const m_bofn;
    unmarshall_fn const m_ufn;
    marshall_fn const m_mfn;

}; // end of class
//............................................................................

template<typename T, typename ID_FIELD>
struct make_erased_fns
{
    using ID_value_type     = typename meta::find_field_def_t<ID_FIELD, T>::value_type;

    static type_erased_fns evaluate ()
    {
        vr_static_assert (std::is_same<T, std::decay_t<T>>::value); // 'T' is already bare

        using bind_ID_T_fn          = void (*) (::sqlite3_stmt * const stmt, ID_value_type const & ID, string_literal_t const ID_name);
        bind_ID_T_fn const bifn     = & bind_ID<ID_value_type>::evaluate;

        using bind_obj_ID_T_fn      = void (*) (::sqlite3_stmt * const stmt, T const & obj, string_literal_t const ID_name);
        bind_obj_ID_T_fn const bofn = & bind_obj_ID<T, ID_FIELD, ID_value_type>::evaluate;

        using umarshall_T_fn        = void (*) (util::str_range const & s, optional<T> & obj_optional);
        umarshall_T_fn const ufn    = & unmarshall<T>;

        using marshall_T_fn         = std::ostream & (*) (T const & obj, std::ostream &);
        marshall_T_fn const mfn     = & json_object_serializer::marshall<T>;

        return
        {
            bind_ID<ID_value_type>::data_type (),
            reinterpret_cast<bind_ID_fn> (bifn),
            reinterpret_cast<bind_obj_ID_fn> (bofn),
            reinterpret_cast<unmarshall_fn> (ufn),
            reinterpret_cast<marshall_fn> (mfn)
        };
    }

}; // end of class
//............................................................................
/*
 * @note the implementation is in "object_DAO.cpp"
 */
class object_DAO_ifc
{
    public: // ...............................................................

        using connection    = sqlite_connection;

        VR_ASSUME_COLD object_DAO_ifc (addr_t const context, bool const rw, std::string const & table_name, std::string const & key_name, string_literal_t const key_data_type);

    protected: // ............................................................

        // read-only:

        tx_guard<connection> tx_begin_ro ();

        VR_ASSUME_HOT void find_as_of (std::type_index const & tix, call_traits<util::date_t>::param date, addr_const_t/* ID_value_type */ const ID, addr_t/* optional<T> */ const optional_obj);

        // read/write:

        tx_guard<connection> tx_begin_rw ();

        VR_ASSUME_HOT bool persist_as_of (std::type_index const & tix, call_traits<util::date_t>::param date, addr_const_t/* T */ const obj);
        VR_ASSUME_HOT bool delete_as_of (std::type_index const & tix, call_traits<util::date_t>::param date, addr_const_t/* ID_value_type */ const ID);


        template<typename CONTEXT>
        VR_FORCEINLINE CONTEXT & context ()
        {
            assert_nonnull (m_context);
            return (* static_cast<CONTEXT *> (m_context));
        }

    private: // ..............................................................

        addr_t/* object_DAO::ifc_context */const m_context;

}; // end of class

} // end of 'impl'
//............................................................................
//............................................................................

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
