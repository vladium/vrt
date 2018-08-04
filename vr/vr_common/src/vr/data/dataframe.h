#pragma once

#include "vr/data/attributes.h"
#include "vr/data/schema_ops.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace data
{
/**
 */
// TODO for 'T' at()s that are typesafe enums, check compatibility b/w 'T' and attr label array
// TODO and/or: attr_type/attribute constructor that takes a typesafe enum type
class dataframe final: noncopyable
{
    private: // ..............................................................

        template<typename T>
        using storage_t     = lower_if_typesafe_enum_t<T>; // local alias for convenience

    public: // ...............................................................

        using size_type     = int64_t;

        dataframe (size_type const row_capacity, attr_schema && schema);
        dataframe (size_type const row_capacity, attr_schema::ptr const & schema);

        // ACCESSORs:

        VR_FORCEINLINE attr_schema::ptr const & schema () const VR_NOEXCEPT;

        size_type row_capacity () const VR_NOEXCEPT;
        size_type const & row_count () const VR_NOEXCEPT;
        attr_schema::size_type col_count () const VR_NOEXCEPT;

        // index lookup:

        template<typename T, bool CHECK_BOUNDS_AND_TYPE = VR_CHECK_INPUT>
        storage_t<T> const *
        at (attr_schema::size_type const index) const;

        template<bool CHECK_BOUNDS = VR_CHECK_INPUT>
        addr_const_t
        at_raw (attr_schema::size_type const index) const;

        // name lookup:

        template<typename T, bool CHECK_NAME_AND_TYPE = VR_CHECK_INPUT>
        storage_t<T> const *
        at (string_literal_t const name) const;

        template<typename T, bool CHECK_BOUNDS_AND_TYPE = VR_CHECK_INPUT>
        storage_t<T> const *
        at (std::string const & name) const
        {
            return at<T, CHECK_BOUNDS_AND_TYPE> (name.c_str ());
        }

        template<bool CHECK_NAME = VR_CHECK_INPUT>
        addr_const_t
        at_raw (string_literal_t const name) const;

        template<bool CHECK_NAME = VR_CHECK_INPUT>
        addr_const_t
        at_raw (std::string const & name) const
        {
            return at_raw<CHECK_NAME> (name.c_str ());
        }


        template<typename V>
        void accept (V && visitor) const;

        // MUTATORs:

        void resize_row_count (size_type const rows);

        /**
         * @note each 'T' can be either a scalar type or a std::array<...>/std::tuple<...>, recursively
         *       (in the latter case the array/tuple is "flattened out")
         *
         * @return index of the added row
         */
        template<bool CHECK_BOUNDS_AND_TYPE = VR_CHECK_INPUT, typename ... Ts>
        VR_ASSUME_HOT size_type
        add_row (Ts && ... vs);

        template<typename T, bool CHECK_BOUNDS_AND_TYPE = VR_CHECK_INPUT>
        storage_t<T> *
        at (attr_schema::size_type const index);

        template<bool CHECK_BOUNDS = VR_CHECK_INPUT>
        addr_t
        at_raw (attr_schema::size_type const index);

        template<typename T, bool CHECK_NAME_AND_TYPE = VR_CHECK_INPUT>
        storage_t<T> *
        at (string_literal_t const name);

        template<typename T, bool CHECK_BOUNDS_AND_TYPE = VR_CHECK_INPUT>
        storage_t<T> *
        at (std::string const & name)
        {
            return const_cast<storage_t<T> *> (const_cast<dataframe const *> (this)->at<T, CHECK_BOUNDS_AND_TYPE> (name));
        }

        template<bool CHECK_NAME = VR_CHECK_INPUT>
        addr_t
        at_raw (string_literal_t const name);

        template<bool CHECK_NAME = VR_CHECK_INPUT>
        addr_t
        at_raw (std::string const & name)
        {
            return at_raw<CHECK_NAME> (name.c_str ());
        }

        template<typename V>
        void accept (V && visitor);

        // FRIENDs:

        friend std::ostream & operator<< (std::ostream & os, dataframe const & obj) VR_NOEXCEPT;
        friend VR_ASSUME_COLD std::string __print__ (dataframe const & obj) VR_NOEXCEPT;

    private: // ..............................................................

        vr_static_assert (std::is_signed<size_type>::value); // have a need to return negative values

        using bitmap_t              = bitset32_t;
        using attr_ops              = impl::attr_offset<bitmap_t>;
        using data_ptr              = std::unique_ptr<bitmap_t [], free_fn_t>;

        static data_ptr allocate_data (attr_schema const & as, size_type const log2_row_capacity VR_IF_DEBUG(, size_type & data_alloc_size));


        attr_schema::ptr const m_schema;
        size_type const m_log2_row_capacity;
        size_type m_row_count { };
        data_ptr const m_data; // util::align_allocate()d, 8-byte alignment
        VR_IF_DEBUG (size_type m_data_alloc_size;) // debug field [note: does not have an initializer by design]

}; // end of class
//............................................................................

inline attr_schema::ptr const &
dataframe::schema () const VR_NOEXCEPT
{
    return m_schema;
}
//............................................................................

inline dataframe::size_type
dataframe::row_capacity () const VR_NOEXCEPT
{
    return (static_cast<size_type> (1) << m_log2_row_capacity);
}

inline dataframe::size_type const &
dataframe::row_count () const VR_NOEXCEPT
{
    return m_row_count;
}

inline attr_schema::size_type
dataframe::col_count () const VR_NOEXCEPT
{
    return m_schema->size ();
}
//............................................................................

template<typename T, bool CHECK_BOUNDS_AND_TYPE>
dataframe::storage_t<T> const *
dataframe::at (attr_schema::size_type const index) const
{
    if (CHECK_BOUNDS_AND_TYPE)
    {
        attribute const & a = m_schema->at<true> (index); // throws if 'index' out of bounds

        if (VR_UNLIKELY (! (castable<T>::atypes & (1 << a.atype ()))))
            throw_x (type_mismatch, "attempt to cast " + print (a.atype ()) + " column to type {" + util::class_name<T> () + '}');
    }

    auto const * VR_RESTRICT data = m_data.get ();
    addr_const_t const r = & data [attr_ops::evaluate (data, index, m_log2_row_capacity)];

    assert_addr_range_contained (r, (sizeof (T) << m_log2_row_capacity), data, m_data_alloc_size, index);

    return static_cast<storage_t<T> const *> (r);
}

template<typename T, bool CHECK_BOUNDS_AND_TYPE>
dataframe::storage_t<T> *
dataframe::at (attr_schema::size_type const index)
{
    return const_cast<storage_t<T> *> (const_cast<dataframe const *> (this)->at<T, CHECK_BOUNDS_AND_TYPE> (index));
}
//............................................................................

template<bool CHECK_BOUNDS>
addr_const_t
dataframe::at_raw (attr_schema::size_type const index) const
{
    if (CHECK_BOUNDS) check_within (index, col_count ());

    auto const * VR_RESTRICT data = m_data.get ();
    addr_const_t const r = & data [attr_ops::evaluate (data, index, m_log2_row_capacity)];

    return r;
}

template<bool CHECK_BOUNDS>
addr_t
dataframe::at_raw (attr_schema::size_type const index)
{
    return const_cast<addr_t> (const_cast<dataframe const *> (this)->at_raw<CHECK_BOUNDS> (index));
}
//............................................................................

template<typename T, bool CHECK_NAME_AND_TYPE>
dataframe::storage_t<T> const *
dataframe::at (string_literal_t const name) const
{
    auto const ap = m_schema->at<CHECK_NAME_AND_TYPE> (name);

    return at<T, CHECK_NAME_AND_TYPE> (std::get<1> (ap));
}

template<typename T, bool CHECK_NAME_AND_TYPE>
dataframe::storage_t<T> *
dataframe::at (string_literal_t const name)
{
    return const_cast<storage_t<T> *> (const_cast<dataframe const *> (this)->at<T, CHECK_NAME_AND_TYPE> (name));
}
//............................................................................

template<bool CHECK_NAME>
addr_const_t
dataframe::at_raw (string_literal_t const name) const
{
    auto const ap = m_schema->at<CHECK_NAME> (name);

    return at_raw<false> (std::get<1> (ap));
}

template<bool CHECK_NAME>
addr_t
dataframe::at_raw (string_literal_t const name)
{
    return const_cast<addr_t> (const_cast<dataframe const *> (this)->at_raw<CHECK_NAME> (name));
}
//............................................................................

template<typename V>
void
dataframe::accept (V && visitor) const
{
    attr_schema const & schema = (* m_schema);

    for (width_type i = 0, i_limit = schema.size (); i < i_limit; ++ i)
    {
        dispatch_on_atype (schema [i].atype (), visitor, * this, i);
    }
}

template<typename V>
void
dataframe::accept (V && visitor)
{
    attr_schema const & schema = (* m_schema);

    for (width_type i = 0, i_limit = schema.size (); i < i_limit; ++ i)
    {
        dispatch_on_atype (schema [i].atype (), visitor, * this, i);
    }
}
//............................................................................
//............................................................................
namespace impl
{

template<typename T, typename = void>
struct arg_width
{
    static constexpr width_type value   = 1;

}; // end of master


template<typename ARRAY>
struct arg_width<ARRAY,
    util::enable_if_t<util::is_std_array<std::decay_t<ARRAY>>::value>>
{
    using ARRAY_bare        = std::decay_t<ARRAY>;
    using ARRAY_value_type  = typename ARRAY_bare::value_type;

    static constexpr width_type value   = util::array_size<ARRAY_bare>::value * arg_width<ARRAY_value_type>::value;

}; // end of master

template<typename TUPLE, width_type I, width_type I_LIMIT>
struct tuple_arg_width; // forward

template<typename TUPLE>
struct arg_width<TUPLE,
    util::enable_if_t<util::is_tuple<std::decay_t<TUPLE>>::value>>
{
    using TUPLE_bare        = std::decay_t<TUPLE>;

    static constexpr width_type value   = tuple_arg_width<TUPLE_bare, 0, std::tuple_size<TUPLE_bare>::value>::value;

}; // end of master
//............................................................................

template<typename TUPLE, width_type I, width_type I_LIMIT>
struct tuple_arg_width
{
    static constexpr width_type value   = arg_width<std::tuple_element_t<I, TUPLE>>::value + tuple_arg_width<TUPLE, (I + 1), I_LIMIT>::value;

}; // end of master

template<typename TUPLE, width_type I_LIMIT>
struct tuple_arg_width<TUPLE, I_LIMIT, I_LIMIT>
{
    static constexpr width_type value   = 0;

}; // end of recursion
//............................................................................

template<typename ... Ts>
struct col_count_impl; // master


template<> // recursion ends
struct col_count_impl<>
{
    static constexpr width_type value   = 0;

}; // end of specialization

template<typename T, typename ... Ts>
struct col_count_impl<T, Ts ...>
{
    static constexpr width_type value   = arg_width<T>::value + col_count_impl<Ts ...>::value;

}; // end of specialization
//............................................................................

template<bool CHECK_BOUNDS_AND_TYPE, width_type INDEX, typename ARRAY>
VR_FORCEINLINE void emit_array (dataframe & df, int64_t const row, ARRAY && a); // forward [defined below]

template<bool CHECK_BOUNDS_AND_TYPE, width_type INDEX, typename TUPLE>
VR_FORCEINLINE void emit_tuple (dataframe & df, int64_t const row, TUPLE && t); // forward [defined below]


template<bool CHECK_BOUNDS_AND_TYPE, width_type INDEX, typename T, typename = void>
struct emit_v_impl
{
    using T_bare            = std::decay_t<T>;

    static VR_FORCEINLINE void evaluate (dataframe & df, int64_t const row, const T & v)
    {
        df.at<T_bare, CHECK_BOUNDS_AND_TYPE> (INDEX)[row] = v;
    }

}; // end of master

template<bool CHECK_BOUNDS_AND_TYPE, width_type INDEX, typename T>
struct emit_v_impl<CHECK_BOUNDS_AND_TYPE, INDEX, T,
    util::enable_if_t<util::is_std_array<std::decay_t<T>>::value>>
{
    static VR_FORCEINLINE void evaluate (dataframe & df, int64_t const row, T && v)
    {
        emit_array<CHECK_BOUNDS_AND_TYPE, INDEX> (df, row, std::forward<T> (v));
    }

}; // end of specialization

template<bool CHECK_BOUNDS_AND_TYPE, width_type INDEX, typename T>
struct emit_v_impl<CHECK_BOUNDS_AND_TYPE, INDEX, T,
    util::enable_if_t<util::is_tuple<std::decay_t<T>>::value>>
{
    static VR_FORCEINLINE void evaluate (dataframe & df, int64_t const row, T && v)
    {
        emit_tuple<CHECK_BOUNDS_AND_TYPE, INDEX> (df, row, std::forward<T> (v));
    }

}; // end of specialization

template<bool CHECK_BOUNDS_AND_TYPE, width_type INDEX, typename T> // add a fn to enable arg type deduction
VR_FORCEINLINE void
emit_v (dataframe & df, int64_t const row, T && v)
{
    emit_v_impl<CHECK_BOUNDS_AND_TYPE, INDEX, T>::evaluate (df, row, v);
}
//............................................................................

template<bool CHECK_BOUNDS_AND_TYPE, width_type INDEX, typename ARRAY, width_type I, width_type I_LIMIT>
struct emit_array_impl
{
    using ARRAY_bare        = std::decay_t<ARRAY>;
    using ARRAY_value_type  = typename ARRAY_bare::value_type;

    static VR_FORCEINLINE void evaluate (dataframe & df, int64_t const row, ARRAY && a)
    {
        emit_v<CHECK_BOUNDS_AND_TYPE, INDEX> (df, row, std::forward<ARRAY> (a)[I]); // [recurse into the array slot type]

        emit_array_impl<CHECK_BOUNDS_AND_TYPE, (INDEX + arg_width<ARRAY_value_type>::value), ARRAY, (I + 1), I_LIMIT>::evaluate (df, row, std::forward<ARRAY> (a)); // recurse
    }

}; // end of master

template<bool CHECK_BOUNDS_AND_TYPE, width_type INDEX, typename ARRAY, width_type I_LIMIT>
struct emit_array_impl<CHECK_BOUNDS_AND_TYPE, INDEX, ARRAY, I_LIMIT, I_LIMIT>
{
    static VR_FORCEINLINE void evaluate (dataframe &, int64_t const, ARRAY &&)  { }

}; // end of recursion


template<bool CHECK_BOUNDS_AND_TYPE, width_type INDEX, typename ARRAY>
VR_FORCEINLINE void
emit_array (dataframe & df, int64_t const row, ARRAY && a)
{
    emit_array_impl<CHECK_BOUNDS_AND_TYPE, INDEX, ARRAY, 0, util::array_size<std::decay_t<ARRAY>>::value>::evaluate (df, row, std::forward<ARRAY> (a));
}
//............................................................................

template<bool CHECK_BOUNDS_AND_TYPE, width_type INDEX, typename TUPLE, width_type I, width_type I_LIMIT>
struct emit_tuple_impl
{
    using TUPLE_bare        = std::decay_t<TUPLE>;
    using TUPLE_element_I   = std::tuple_element_t<I, TUPLE_bare>;

    static VR_FORCEINLINE void evaluate (dataframe & df, int64_t const row, TUPLE && t)
    {
        emit_v<CHECK_BOUNDS_AND_TYPE, INDEX> (df, row, std::get<I> (std::forward<TUPLE> (t))); // [recurse into the tuple slot type]

        emit_tuple_impl<CHECK_BOUNDS_AND_TYPE, (INDEX + arg_width<TUPLE_element_I>::value), TUPLE, (I + 1), I_LIMIT>::evaluate (df, row, std::forward<TUPLE> (t)); // recurse
    }

}; // end of master

template<bool CHECK_BOUNDS_AND_TYPE, width_type INDEX, typename TUPLE, width_type I_LIMIT>
struct emit_tuple_impl<CHECK_BOUNDS_AND_TYPE, INDEX, TUPLE, I_LIMIT, I_LIMIT>
{
    static VR_FORCEINLINE void evaluate (dataframe &, int64_t const, TUPLE &&)  { }

}; // end of recursion


template<bool CHECK_BOUNDS_AND_TYPE, width_type INDEX, typename TUPLE>
VR_FORCEINLINE void
emit_tuple (dataframe & df, int64_t const row, TUPLE && t)
{
    emit_tuple_impl<CHECK_BOUNDS_AND_TYPE, INDEX, TUPLE, 0, std::tuple_size<std::decay_t<TUPLE>>::value>::evaluate (df, row, std::forward<TUPLE> (t));
}
//............................................................................

template<bool CHECK_BOUNDS_AND_TYPE, width_type INDEX, typename ... Ts>
struct add_row_impl; // master


template<bool CHECK_BOUNDS_AND_TYPE, width_type INDEX> // recursion ends
struct add_row_impl<CHECK_BOUNDS_AND_TYPE, INDEX>
{
    static VR_FORCEINLINE void evaluate (dataframe &, int64_t const)    { }

}; // end of recursion

template<bool CHECK_BOUNDS_AND_TYPE, width_type INDEX, typename T, typename ... Ts>
struct add_row_impl<CHECK_BOUNDS_AND_TYPE, INDEX, T, Ts ...>
{
    static VR_FORCEINLINE void evaluate (dataframe & df, int64_t const row, T && v, Ts && ... vs)
    {
        emit_v<CHECK_BOUNDS_AND_TYPE, INDEX> (df, row, v); //  TODO forward 'v'

        add_row_impl<CHECK_BOUNDS_AND_TYPE, (INDEX + arg_width<T>::value), Ts ...>::evaluate (df, row, std::forward<Ts> (vs) ...); // recurse
    }

}; // end of specialization

} // end of 'impl'
//............................................................................
//............................................................................

template<bool CHECK_BOUNDS_AND_TYPE, typename ... Ts>
dataframe::size_type
dataframe::add_row (Ts && ... vs)
{
    auto const row = m_row_count;

    if (CHECK_BOUNDS_AND_TYPE)
    {
        constexpr auto Ts_width = impl::col_count_impl<Ts ...>::value;
        check_eq (col_count (), Ts_width);

        if (VR_UNLIKELY (row == row_capacity ()))
            throw_x (out_of_bounds, "row capacity (" + string_cast (row) + ") reached");
    }

    impl::add_row_impl<CHECK_BOUNDS_AND_TYPE, 0, Ts ...>::evaluate (* this, row, std::forward<Ts> (vs) ...);
    m_row_count = (row + 1);

    return row;
}
//............................................................................

inline std::ostream &
operator<< (std::ostream & os, dataframe const & obj) VR_NOEXCEPT
{
    return os << __print__ (obj);
}

} // end of 'data'
} // end of namespace
//----------------------------------------------------------------------------
