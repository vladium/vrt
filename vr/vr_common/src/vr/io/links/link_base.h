#pragma once

#include "vr/arg_map.h"
#include "vr/enums.h"
#include "vr/fields.h"
#include "vr/io/mapped_ring_buffer.h"
#include "vr/io/mapped_tape_buffer.h"
#include "vr/io/net/defs.h" // _filtered_
#include "vr/meta/objects.h"
#include "vr/tags.h"

#include "vr/io/links/utility.h" // blank_fill() TODO rm

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
//............................................................................

VR_ENUM (link_state,
    (
        created,
        open,
        closed
    ),
    printable

); // end of enum
//............................................................................

using link_schema   = meta::make_schema_t
<
    meta::fdef_<link_state::enum_t,     _state_>,
    meta::fdef_<timestamp_t,            _ts_last_recv_>,
    meta::fdef_<timestamp_t,            _ts_last_send_>
>;
//............................................................................

struct recv_arg_map: public arg_map // "strong typedef"
{
    using arg_map::arg_map;

}; // end of class

struct send_arg_map: public arg_map // "strong typedef"
{
    using arg_map::arg_map;

}; // end of class

//............................................................................
//............................................................................
namespace impl
{
//............................................................................

vr_static_assert (EAGAIN == EWOULDBLOCK); // Linux

//............................................................................

template<typename BUFFER_TAG = _ring_>
struct buffer_impl_traits
{
    using impl_type     = mapped_ring_buffer;

    static constexpr bool requires_file ()  { return false; } // TODO this is best associated with 'impl_type' itself

}; // end of master

template<>
struct buffer_impl_traits<_tape_>
{
    using impl_type     = mapped_tape_buffer;

    static constexpr bool requires_file ()  { return true; } // TODO this is best associated with 'impl_type' itself

}; // end of specialization
//............................................................................

struct default_traits_base
{
    static constexpr mode::enum_t link_mode ()      { return mode::none; }

    using buffer_tag        = _ring_;
    using buffer_impl_type  = buffer_impl_traits<buffer_tag>::impl_type;
    using buffer_size_type  = buffer_impl_type::size_type;
    static constexpr bool buffer_requires_file ()   { return buffer_impl_traits<buffer_tag>::requires_file (); }

    static constexpr bool is_filtered ()            { return false; }
    static constexpr bool is_timestamped ()         { return false; }

}; // end of traits

template<mode::enum_t MODE>
struct default_traits: public default_traits_base { };

//............................................................................

template<mode::enum_t MODE, typename ... ASPECTs>
struct find_buffer_tag
{
    using type          = util::if_t<util::contains<_tape_, ASPECTs ...>::value, _tape_, typename default_traits<MODE>::buffer_tag>;

}; // end of metafunction
//............................................................................

template<typename T, typename = void>
struct link_mode_of
{
    static constexpr mode::enum_t value = mode::none;

}; // end of master

template<typename T>
struct link_mode_of<T,
    util::void_t<decltype (T::link_mode ()) > >
{
    static constexpr mode::enum_t value = T::link_mode ();

}; // end of specialization
//............................................................................
/*
 * selects first (T::link_mode() == MODE) match in 'Ts' (or 'T_DEFAULT' if no match)
 */
template<mode::enum_t MODE, typename T_DEFAULT, typename ... Ts>
struct find_mode; // forward


template<mode::enum_t MODE, typename T_DEFAULT>
struct find_mode<MODE, T_DEFAULT>
{
    using type          = T_DEFAULT;

}; // end of specialization

template<mode::enum_t MODE, typename T_DEFAULT, typename T, typename ... Ts>
struct find_mode<MODE, T_DEFAULT, T, Ts ...>
{
    using type          = util::if_t<(link_mode_of<T>::value == MODE), T, typename find_mode<MODE, T_DEFAULT, Ts ...>::type>;

}; // end of specialization
//............................................................................

#define vr_DEFINE_MODE_BASE(NAME) \
    \
    struct NAME##_base \
    { \
        static constexpr mode::enum_t link_mode ()  { return mode:: NAME ; } \
        \
    }; \
    /* */

#define vr_DEFINE_MODE(NAME) \
    \
    template<typename ... ASPECTs> \
    struct NAME : public impl:: NAME##_base \
    { \
        using buffer_tag        = typename impl::find_buffer_tag<mode:: NAME , ASPECTs ...>::type; \
        using buffer_traits     = impl::buffer_impl_traits<buffer_tag>; \
        \
        using buffer_impl_type  = typename buffer_traits::impl_type; \
        using buffer_size_type  = typename buffer_impl_type::size_type; \
        static constexpr bool buffer_requires_file ()   { return buffer_traits::requires_file (); } \
        \
        static constexpr bool is_filtered ()            { return util::contains<_filter_, ASPECTs ...>::value; } \
        static constexpr bool is_timestamped ()         { return util::contains<_timestamp_, ASPECTs ...>::value; } \
        \
    }; \
    /* */


vr_DEFINE_MODE_BASE (recv)
vr_DEFINE_MODE_BASE (send)

#undef vr_DEFINE_MODE_BASE
//............................................................................
/*
 * "traits generator" metafunction
 */
template<typename ... ASPECTs>
struct link_traits
{
    using recv_traits   = typename find_mode<mode::recv, default_traits<mode::recv>, ASPECTs ...>::type;
    using send_traits   = typename find_mode<mode::send, default_traits<mode::send>, ASPECTs ...>::type;

}; // end of traits
//............................................................................

template<typename ... ASPECTs>
struct declared_mode
{
    using lt            = link_traits<ASPECTs ...>;

    static constexpr mode::enum_t value = (static_cast<mode::enum_t> (lt::recv_traits::link_mode () | lt::send_traits::link_mode ()));

}; // end of metafunction
//............................................................................

template<typename ... ASPECTs>
struct recv_is_filtered
{
    using lt            = link_traits<ASPECTs ...>;

    static constexpr bool value         = lt::recv_traits::is_filtered ();

}; // end of metafunction
//............................................................................
/*
 * empty class for when a recv/send aspect is absent
 *
 * triggers compile-time errors on attempts to use the API without guards
 */
template<mode::enum_t MODE>
struct elided_buffer
{
    using size_type             = signed_size_t;

    template<typename ... ARGs>
    elided_buffer (ARGs && ... args)
    {
    }

    // accessors:

    template<typename LAZY = void>
    addr_const_t r_position () const
    {
        static_assert (util::lazy_false<LAZY>::value, VR_UNGUARDED_METHOD_ERROR);
        VR_ASSUME_UNREACHABLE ();
    }

    template<typename LAZY = void>
    size_type size () const
    {
        static_assert (util::lazy_false<LAZY>::value, VR_UNGUARDED_METHOD_ERROR);
        VR_ASSUME_UNREACHABLE ();
    }

    template<typename LAZY = void>
    size_type w_window () const
    {
        static_assert (util::lazy_false<LAZY>::value, VR_UNGUARDED_METHOD_ERROR);
        VR_ASSUME_UNREACHABLE ();
    }

    // mutators:

    template<typename LAZY = void>
    addr_t w_position ()
    {
        static_assert (util::lazy_false<LAZY>::value, VR_UNGUARDED_METHOD_ERROR);
        VR_ASSUME_UNREACHABLE ();
    }

    template<typename LAZY = void>
    addr_const_t r_advance (size_type const step)
    {
        static_assert (util::lazy_false<LAZY>::value, VR_UNGUARDED_METHOD_ERROR);
        VR_ASSUME_UNREACHABLE ();
    }

    template<typename LAZY = void>
    addr_t w_advance (size_type const step)
    {
        static_assert (util::lazy_false<LAZY>::value, VR_UNGUARDED_METHOD_ERROR);
        VR_ASSUME_UNREACHABLE ();
    }

}; // end of class
//............................................................................
/*
 * tag a 'TRAITS::buffer_type' with 'MODE' to allow multiple inheritance in 'lic' below
 */
template<mode::enum_t MODE, typename TRAITS>
struct tagged_buffer_impl: public util::if_t<(std::is_same<default_traits<MODE>, TRAITS>::value), elided_buffer<MODE>, typename TRAITS::buffer_impl_type>
{
    using super         = util::if_t<(std::is_same<default_traits<MODE>, TRAITS>::value), elided_buffer<MODE>, typename TRAITS::buffer_impl_type>;

    using traits        = TRAITS;


    using super::super; // inherit constructors

}; // end of class
//............................................................................
/*
 * a traits-like converter mf to simplify 'link_impl' definition
 */
template<typename ... ASPECTs>
struct link_a2t
{
    using recv_impl     = tagged_buffer_impl<mode::recv, typename link_traits<ASPECTs ...>::recv_traits>;
    using send_impl     = tagged_buffer_impl<mode::send, typename link_traits<ASPECTs ...>::send_traits>;

    static constexpr bool has_state ()          { return util::contains<_state_, ASPECTs ...>::value; }
    static constexpr bool has_recv_filter ()    { return recv_impl::traits::is_filtered (); }
    static constexpr bool has_ts_last_recv ()   { return recv_impl::traits::is_timestamped (); }
    static constexpr bool has_ts_last_send ()   { return send_impl::traits::is_timestamped (); }

    using field_seq     = meta::make_schema_t
                        <
                            meta::fdef_<link_state::enum_t,     _state_,            meta::elide<(! has_state ())>>,
                            /* '_filter_' interpretation depends on the link type */
                            meta::fdef_<timestamp_t,            _ts_last_recv_,     meta::elide<(! has_ts_last_recv ())>>,
                            meta::fdef_<timestamp_t,            _ts_last_send_,     meta::elide<(! has_ts_last_send ())>>
                        >;

    using fields        = typename meta::make_compact_struct_t<field_seq>;

    using recv_buffer_tag    = typename recv_impl::traits::buffer_tag;
    using send_buffer_tag    = typename send_impl::traits::buffer_tag;

    using recv_buffer   = typename recv_impl::traits::buffer_impl_type;
    using send_buffer   = typename send_impl::traits::buffer_impl_type;

    static constexpr bool recv_requires_file () { return recv_impl::traits::buffer_requires_file (); }
    static constexpr bool send_requires_file () { return send_impl::traits::buffer_requires_file (); }

}; // end of metafunction
//............................................................................

template<typename ... ASPECTs>
class link_impl: public link_a2t<ASPECTs ...>::fields,
                 public link_a2t<ASPECTs ...>::recv_impl, // EBO
                 public link_a2t<ASPECTs ...>::send_impl  // EBO
{
    private: // ..............................................................

        using traits        = link_a2t<ASPECTs ...>;

        using this_type     = link_impl<ASPECTs ...>;

    protected: // ............................................................

        // these need to be accessible to 'link_base_impl':

        using recv_impl     = typename traits::recv_impl;
        using send_impl     = typename traits::send_impl;

        // ACCESSORs:

        VR_FORCEINLINE recv_impl const & recv_ifc () const
        {
            vr_static_assert (link_mode () & mode::recv); // catch ifc usage errors early at compile-time

            return (* static_cast<recv_impl const *> (this));
        }

        VR_FORCEINLINE send_impl const & send_ifc () const
        {
            vr_static_assert (link_mode () & mode::send); // catch ifc usage errors early at compile-time

            return (* static_cast<send_impl const *> (this));
        }

        // MUTATORs:

        VR_FORCEINLINE recv_impl & recv_ifc ()
        {
            return const_cast<recv_impl &> (const_cast<this_type const *> (this)->recv_ifc ());
        }

        VR_FORCEINLINE send_impl & send_ifc ()
        {
            return const_cast<send_impl &> (const_cast<this_type const *> (this)->send_ifc ());
        }

    public: // ...............................................................

        using recv_buffer_tag    = typename traits::recv_buffer_tag;
        using send_buffer_tag    = typename traits::send_buffer_tag;

        static constexpr bool recv_requires_file () { return traits::recv_requires_file (); }
        static constexpr bool send_requires_file () { return traits::send_requires_file (); }

        static constexpr mode::enum_t link_mode ()  { return static_cast<mode::enum_t> (recv_impl::traits::link_mode () | send_impl::traits::link_mode ()); }

        vr_static_assert (link_mode () == impl::declared_mode<ASPECTs...>::value);


        static constexpr bool has_state ()          { return traits::has_state (); }
        static constexpr bool has_recv_filter ()    { return traits::has_recv_filter (); }
        static constexpr bool has_ts_last_recv ()   { return traits::has_ts_last_recv (); }
        static constexpr bool has_ts_last_send ()   { return traits::has_ts_last_send (); }

        // TODO tuple forwarding in constructors instead of 'arg_map'?

        /*
         * bi-directional (recv/send) link
         */
        link_impl (recv_arg_map const & recv_args, send_arg_map const & send_args) :
            recv_impl (recv_args),
            send_impl (send_args)
        {
            if (has_state ()) field<_state_> (* this) = link_state::created;
            if (has_ts_last_recv ()) field<_ts_last_recv_> (* this) = 0;
            if (has_ts_last_send ()) field<_ts_last_send_> (* this) = 0;
        }

        /*
         * uni-directional (recv) link
         */
        link_impl (recv_arg_map const & recv_args) :
            recv_impl (recv_args),
            send_impl (arg_map { }) // [no-op]
        {
            if (has_state ()) field<_state_> (* this) = link_state::created;
            if (has_ts_last_recv ()) field<_ts_last_recv_> (* this) = 0;
        }

        /*
         * uni-directional (send) link
         */
        link_impl (send_arg_map const & send_args) :
            recv_impl (arg_map { }), // [no-op]
            send_impl (send_args)
        {
            if (has_state ()) field<_state_> (* this) = link_state::created;
            if (has_ts_last_send ()) field<_ts_last_send_> (* this) = 0;
        }


        // ACCESSORs:

        VR_FORCEINLINE link_state::enum_t const & state () const
        {
            // TODO static_assert (has_state (), VR_UNGUARDED_FIELD_ERROR);

            return field<_state_> (* this);
        }

        VR_FORCEINLINE timestamp_t const & ts_last_recv () const
        {
            // TODO static_assert (has_ts_last_recv (), VR_UNGUARDED_FIELD_ERROR);

            return field<_ts_last_recv_> (* this);
        }

        VR_FORCEINLINE timestamp_t const & ts_last_send () const
        {
            // TODO static_assert (has_ts_last_send (), VR_UNGUARDED_FIELD_ERROR);

            return field<_ts_last_send_> (* this);
        }

        // MUTATORs:

        VR_FORCEINLINE link_state::enum_t & state ()
        {
            return const_cast<link_state::enum_t &> (const_cast<this_type const *> (this)->state ());
        }

        VR_FORCEINLINE timestamp_t & ts_last_recv ()
        {
            return const_cast<timestamp_t &> (const_cast<this_type const *> (this)->ts_last_recv ());
        }

        VR_FORCEINLINE timestamp_t & ts_last_send ()
        {
            return const_cast<timestamp_t &> (const_cast<this_type const *> (this)->ts_last_send ());
        }

        // recv interface:

        VR_FORCEINLINE window_t recv_w_window () const
        {
            return recv_ifc ().w_window ();
        }

        /// std::pair<addr_const_t, capacity_t> recv_poll ();

        /**
         * indicate that 'len' bytes of currently buffered "recv" data have been consumed and can be discarded/committed
         */
        VR_ASSUME_HOT void recv_flush (capacity_t const len)
        {
            recv_ifc ().r_advance (len);
        }

        // send interface:

        VR_FORCEINLINE window_t send_w_window () const
        {
            return send_ifc ().w_window ();
        }

        /**
         * @return memory address of the start of the allocated bytes
         */
        VR_ASSUME_HOT addr_t send_allocate (capacity_t const len, bool const blank_init = true) // TODO factor out 'blank' or other kinds of 'init'
        {
            send_impl & ifc = send_ifc ();

            assert_positive (ifc.w_window ()); // somewhat redundant ('w_advance ()' will check/fail also)
            assert_le (len, ifc.w_window ());

            // [note the careful logic here: can't do addr arithmetic on w-position,
            //  need to re-acquire r-position after w-advancing instead]

            auto const sz = ifc.size (); // *before* w_advance()

            ifc.w_advance (len);
            addr_t const r_addr = addr_plus (const_cast<addr_t> (ifc.r_position ()), sz);

            if (blank_init) blank_fill (r_addr, len);

            return r_addr;
        }

        /// capacity_t send_flush (capacity_t const len);

}; // end of class
//............................................................................

template<typename DERIVED, typename ... ASPECTs>
class link_base_impl: public link_impl<ASPECTs ...>
{
    private: // ..............................................................

        using super         = link_impl<ASPECTs ...>;

    protected: // ............................................................

        // these need to be accessible to 'DERIVED':

        using typename super::recv_impl;
        using typename super::send_impl;

        using super::recv_ifc;
        using super::send_ifc;

    public: // ...............................................................

        using super::super; // inherit constructors


        // recv interface:

        /**
         * receive-poll the link (without blocking)
         *
         * @return cumulative count of bytes in the recv buffer and their memory address
         *
         * @note if a link is polled multiple times without an intervening @ref recv_flush() the implementation guarantees
         *       that the returned address points to a contiguous region of all bytes recv'ed so far (even though the
         *       actual address value may change from one call to the next)
         */
        VR_ASSUME_HOT std::pair<addr_const_t, capacity_t> recv_poll ()
        {
            recv_impl & ifc = recv_ifc ();

            if (VR_LIKELY (ifc.w_window () > 0)) // TODO this runtime check can be elided for certain buffer types
                return static_cast<DERIVED *> (this)->recv_poll_impl (); // implemented by 'DERIVED', presumably via 'recv_ifc ()'
            else
                return { ifc.r_position (), ifc.size () };
        }

        // send interface:

        /**
         * send 'len' byte(s) of currently buffered send data (without blocking)
         *
         * TODO doc return value semantics
         */
        VR_ASSUME_HOT capacity_t send_flush (capacity_t const len)
        {
            assert_positive (len); // shouldn't have to write 0 bytes

            return static_cast<DERIVED *> (this)->send_flush_impl (len); // implemented by 'DERIVED', presumably via 'send_ifc ()'
        }

}; // end of class

} // end of 'impl'
//............................................................................
//............................................................................

vr_DEFINE_MODE (recv)
vr_DEFINE_MODE (send)

#undef vr_DEFINE_MODE
//............................................................................
/**
 * a "non-blocking socket"-like abstraction that supports: TODO
 */
template<typename DERIVED, typename ... ASPECTs>
using link_base             = impl::link_base_impl<DERIVED, ASPECTs ...>;

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
