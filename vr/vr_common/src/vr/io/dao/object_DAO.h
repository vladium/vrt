#pragma once

#include "vr/io/dao/object_DAO_ifc.h"
#include "vr/io/meta/persist_traits.h"
#include "vr/settings_fwd.h"
#include "vr/meta/objects.h"
#include "vr/util/di/component.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
class sql_connection_factory; // forward

/**
 *
 * based on lessons from some past impls, this API is chosen to return query
 * results by value: this does away with the need to manage persistence
 * "sessions" as in more elaborate/caching designs
 */
class object_DAO final: public util::di::component
{
    private: // ..............................................................

        using ifc_impl      = impl::object_DAO_ifc;

    public: // ...............................................................

        using connection    = sqlite_connection;
        using date_t        = util::date_t;

        /**
         * read-only interface for 'T'
         */
        template<typename T, typename ID_FIELD>
        struct ro_ifc_impl: ifc_impl
        {
            using ID_value_type = typename meta::find_field_def_t<ID_FIELD, T>::value_type;

            /**
             * @note using this scoped guard is optional (improves performance for batches of operations)
             *
             * @return a TX guard bound to a TLS connection instance
             */
            tx_guard<connection> tx_begin ();

            /*
             * TODO return the 'vb' of the result (useful for checking "staleness" for some Ts)? an alternative is
             *      to provide a separate "exact date" query
             */
            VR_ASSUME_HOT optional<T> find_as_of (call_traits<date_t>::param date, typename call_traits<ID_value_type>::param ID); /* const */

        }; // end of nested class

        /**
         * read/write inteface (extension of read-only) for 'T'
         */
        template<typename T, typename ID_FIELD>
        struct rw_ifc_impl final: ro_ifc_impl<T, ID_FIELD>
        {
            using ID_value_type = typename meta::find_field_def_t<ID_FIELD, T>::value_type;

            /**
             * @note using this scoped guard is optional (improves performance for batches of operations)
             *
             * @return a TX guard bound to a TLS connection instance
             */
            tx_guard<connection> tx_begin ();

            /**
             * @return 'true' if 'obj' got persisted as a first/new version starting on 'date', 'false' otherwise
             */
            VR_ASSUME_HOT bool persist_as_of (call_traits<date_t>::param date, T const & obj);

            /**
             * "deletes" a current object by setting its end validity date to 'date'
             *
             * @return 'true' if a current object identified by 'ID' existed
             */
            VR_ASSUME_HOT bool delete_as_of (call_traits<date_t>::param date, typename call_traits<ID_value_type>::param ID);

        }; // end of nested class


        VR_ASSUME_COLD object_DAO (settings const & cfg);
        ~object_DAO ();


        // ACCESSORs (rw, ro modes):

        template<typename T, typename ID_FIELD = typename persist_traits<T>::ID_type>
        ro_ifc_impl<std::decay_t<T>, ID_FIELD> & ro_ifc () const;

        // MUTATORs (rw mode):

        template<typename T, typename ID_FIELD = typename persist_traits<T>::ID_type>
        rw_ifc_impl<std::decay_t<T>, ID_FIELD> & rw_ifc ();

        bool read_only () const;

    private: // ..............................................................

        friend class impl::object_DAO_ifc; // grant access to 'ifc_context' and 'pimpl'

        class ifc_context; // forward
        class pimpl; // forward


        addr_t ro_ifc (std::type_index const & tix, std::string && key_name, impl::type_erased_fns && fns) const;
        addr_t rw_ifc (std::type_index const & tix, std::string && key_name, impl::type_erased_fns && fns);


        sql_connection_factory * m_sql_factory { }; // [dep]

        std::unique_ptr<pimpl> const m_impl; // type-erased impl

}; // end of class
//............................................................................

template<typename T, typename ID_FIELD>
object_DAO::ro_ifc_impl<std::decay_t<T>, ID_FIELD> &
object_DAO::ro_ifc () const
{
    using T_bare        = std::decay_t<T>;
    vr_static_assert (has_field<ID_FIELD, T_bare> ());

    return (* static_cast<ro_ifc_impl<T_bare, ID_FIELD> *> (ro_ifc (std::type_index { typeid (T_bare) }, print_tag<ID_FIELD> (), impl::make_erased_fns<T_bare, ID_FIELD>::evaluate ())));
}
//............................................................................

template<typename T, typename ID_FIELD>
object_DAO::rw_ifc_impl<std::decay_t<T>, ID_FIELD> &
object_DAO::rw_ifc ()
{
    check_condition (! read_only ());

    using T_bare        = std::decay_t<T>;
    vr_static_assert (has_field<ID_FIELD, T_bare> ());

    return (* static_cast<rw_ifc_impl<T_bare, ID_FIELD> *> (rw_ifc (std::type_index { typeid (T_bare) }, print_tag<ID_FIELD> (), impl::make_erased_fns<T_bare, ID_FIELD>::evaluate ())));
}
//............................................................................
//............................................................................

template<typename T, typename ID_FIELD>
tx_guard<object_DAO::connection>
object_DAO::ro_ifc_impl<T, ID_FIELD>::tx_begin ()
{
    return ifc_impl::tx_begin_ro ();
}
//............................................................................

template<typename T, typename ID_FIELD>
optional<T>
object_DAO::ro_ifc_impl<T, ID_FIELD>::find_as_of (call_traits<date_t>::param date, typename call_traits<ID_value_type>::param ID)
{
    vr_static_assert (std::is_same<T, std::decay_t<T>>::value); // 'T' is already bare

    optional<T> r; // note: 'T' is not default-initialized here
    ifc_impl::find_as_of (std::type_index { typeid (T) }, date, & ID, & r);

    return r;
}
//............................................................................
//............................................................................

template<typename T, typename ID_FIELD>
tx_guard<object_DAO::connection>
object_DAO::rw_ifc_impl<T, ID_FIELD>::tx_begin ()
{
    return ifc_impl::tx_begin_rw ();
}
//............................................................................

template<typename T, typename ID_FIELD>
bool
object_DAO::rw_ifc_impl<T, ID_FIELD>::persist_as_of (call_traits<date_t>::param date, T const & obj)
{
    vr_static_assert (std::is_same<T, std::decay_t<T>>::value); // 'T' is already bare

    return ifc_impl::persist_as_of (std::type_index { typeid (T) }, date, & obj);
}

template<typename T, typename ID_FIELD>
bool
object_DAO::rw_ifc_impl<T, ID_FIELD>::delete_as_of (call_traits<date_t>::param date, typename call_traits<ID_value_type>::param ID)
{
    vr_static_assert (std::is_same<T, std::decay_t<T>>::value); // 'T' is already bare

    return ifc_impl::delete_as_of (std::type_index { typeid (T) }, date, & ID);
}

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
