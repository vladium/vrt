#pragma once

#include "vr/market/sources/asx/itch/ITCH_visitor.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
namespace ASX
{
//............................................................................
//............................................................................
namespace impl
{

// TODO some branches could be eliminated if the pipeline model could be simplified (get rid of pre-/post-visits?)

template<typename VTUPLE, std::size_t I, std::size_t I_LIMIT>
struct pipeline_dispatch_impl
{
    template<typename CTX> // pre_packet
    static VR_FORCEINLINE void visit_forward (VTUPLE & visitors, pre_packet const msg_count, CTX & ctx)
    {
        std::get<I> (visitors).visit (msg_count, ctx);

        pipeline_dispatch_impl<VTUPLE, (I + 1), I_LIMIT>::visit_forward (visitors, msg_count, ctx);
    }

    template<typename CTX> // pre_message
    static VR_FORCEINLINE uint32_t visit_forward (VTUPLE & visitors, pre_message const msg_type, addr_const_t const msg, CTX & ctx)
    {
        if (! std::get<I> (visitors).visit (msg_type, msg, ctx))
            return I; // visit limit determined

        return pipeline_dispatch_impl<VTUPLE, (I + 1), I_LIMIT>::visit_forward (visitors, msg_type, msg, ctx);
    }

    template<typename MSG, typename CTX> // visit (MSG)
    static VR_FORCEINLINE uint32_t visit_forward (VTUPLE & visitors, MSG const & msg, CTX & ctx, uint32_t const v_limit)
    {
        if (I == v_limit)
            return I; // visit limit reached

        if (! std::get<I> (visitors).visit (msg, ctx))
            return (I + 1); // visit limit possibly reduced [note: if 'I'th visitor returned 'false', it was still invoked and hence must be post-visited]

        return pipeline_dispatch_impl<VTUPLE, (I + 1), I_LIMIT>::visit_forward (visitors, msg, ctx, v_limit);
    }

    template<typename CTX> // post_message
    static VR_FORCEINLINE void visit_reverse (VTUPLE & visitors, post_message const msg_type, CTX & ctx, uint32_t const v_limit)
    {
        if ((I_LIMIT - 1 - I) < v_limit) // don't post-visit outside of visit limits
            std::get<(I_LIMIT - 1 - I)> (visitors).visit (msg_type, ctx);

        pipeline_dispatch_impl<VTUPLE, (I + 1), I_LIMIT>::visit_reverse (visitors, msg_type, ctx, v_limit);
    }

    template<typename CTX> // post_packet
    static VR_FORCEINLINE void visit_reverse (VTUPLE & visitors, post_packet const msg_count, CTX & ctx)
    {
        std::get<(I_LIMIT - 1 - I)> (visitors).visit (msg_count, ctx);

        pipeline_dispatch_impl<VTUPLE, (I + 1), I_LIMIT>::visit_reverse (visitors, msg_count, ctx);
    }

}; // end of master

template<typename VTUPLE, std::size_t I_LIMIT> // end of recursion
struct pipeline_dispatch_impl<VTUPLE, I_LIMIT, I_LIMIT>
{
    template<typename CTX> // pre_packet
    static VR_FORCEINLINE void visit_forward (VTUPLE & visitors, pre_packet const msg_count, CTX & ctx)
    {
    }

    template<typename CTX> // pre_message
    static VR_FORCEINLINE uint32_t visit_forward (VTUPLE & visitors, pre_message const msg_type, addr_const_t const msg, CTX & ctx)
    {
        return I_LIMIT;
    }

    template<typename MSG, typename CTX> // visit (MSG)
    static VR_FORCEINLINE uint32_t visit_forward (VTUPLE & visitors, MSG const & msg, CTX & ctx, uint32_t const v_limit)
    {
        return I_LIMIT;
    }

    template<typename CTX> // post_message
    static VR_FORCEINLINE void visit_reverse (VTUPLE & visitors, post_message const msg_type, CTX & ctx, uint32_t const v_limit)
    {
    }

    template<typename CTX> // post_packet
    static VR_FORCEINLINE void visit_reverse (VTUPLE & visitors, post_packet const msg_count, CTX & ctx)
    {
    }

}; // end of recursion
//............................................................................

template<typename VTUPLE>
struct pipeline_dispatch
{
    vr_static_assert (util::is_tuple<VTUPLE>::value);

    static constexpr uint32_t depth ()           { return std::tuple_size<VTUPLE>::value; }

    vr_static_assert (depth () > 0);


    template<typename CTX> // pre_packet
    static VR_FORCEINLINE void visit_forward (VTUPLE & visitors, pre_packet const msg_count, CTX & ctx)
    {
        pipeline_dispatch_impl<VTUPLE, 0, depth ()>::visit_forward (visitors, msg_count, ctx);
    }

    template<typename CTX> // pre_message
    static VR_FORCEINLINE uint32_t visit_forward (VTUPLE & visitors, pre_message const msg_type, addr_const_t const msg, CTX & ctx)
    {
        return pipeline_dispatch_impl<VTUPLE, 0, depth ()>::visit_forward (visitors, msg_type, msg, ctx);
    }

    template<typename MSG, typename CTX> // visit (MSG)
    static VR_FORCEINLINE uint32_t visit_forward (VTUPLE & visitors, MSG const & msg, CTX & ctx, uint32_t const v_limit)
    {
        return pipeline_dispatch_impl<VTUPLE, 0, depth ()>::visit_forward (visitors, msg, ctx, v_limit);
    }

    template<typename CTX> // post_message
    static VR_FORCEINLINE void visit_reverse (VTUPLE & visitors, post_message const msg_type, CTX & ctx, uint32_t const v_limit)
    {
        pipeline_dispatch_impl<VTUPLE, 0, depth ()>::visit_reverse (visitors, msg_type, ctx, v_limit);
    }

    template<typename CTX> // post_packet
    static VR_FORCEINLINE void visit_reverse (VTUPLE & visitors, post_packet const msg_count, CTX & ctx)
    {
        pipeline_dispatch_impl<VTUPLE, 0, depth ()>::visit_reverse (visitors, msg_count, ctx);
    }

}; // end of class

} // end of 'impl'
//............................................................................
//............................................................................
/**
 * a composite visitor, not subclassable TODO explain
 *
 * TODO doc pre/post-visit hook semantics
 */
template<typename ... VISITORs>
class ITCH_pipeline: public ITCH_visitor<ITCH_pipeline<VISITORs ...>>
{
    private: // ..............................................................

        using this_type     = ITCH_pipeline<VISITORs ...>;
        using super         = ITCH_visitor<this_type>;

        using vtuple        = std::tuple<VISITORs ...>;
        using vdispatch     = impl::pipeline_dispatch<vtuple>;

    public: // ...............................................................

        // TODO assert required VISITORs traits (constr signatures, visit() fn traits, etc) as early as possible

        // TODO deal with movable visitor requirement here
        /*
         * note: passing the same copy of 'args' into all visitors by design
         */
        ITCH_pipeline (arg_map const & args) :
            m_visitors { (void (sizeof (VISITORs)), args) ... } // trick: comma operator used to reference 'VISITORs' but not used otherwise
        {
        }

// extra copy/move here?:
//        /*
//         * note: passing the same copy of 'args' into all visitors by design
//         */
//        ITCH_pipeline (arg_map const & args) :
//            m_visitors { VISITORs { args } ... }
//        {
//        }


        // ACCESSORs:

        template<typename VISITOR>
        VISITOR const & get () const
        {
            constexpr int32_t index     = util::index_of<VISITOR, VISITORs ...>::value;
            vr_static_assert (index >= 0);

            return std::get<index> (m_visitors);
        }

        // MUTATORs:

        template<typename VISITOR>
        VISITOR & get ()
        {
            return const_cast<VISITOR &> (const_cast<this_type const *> (this)->get<VISITOR> ());
        }

        // overridden visits:

        using super::visit; // this is somewhat redundant because this visitor overrides every 'super' visit

        // pre_/post_packet visits:

        template<typename CTX>
        VR_FORCEINLINE void visit (pre_packet const msg_count, CTX & ctx)
        {
            vdispatch::visit_forward (m_visitors, msg_count, ctx);
        }

        template<typename CTX>
        VR_FORCEINLINE void visit (post_packet const msg_count, CTX & ctx)
        {
            vdispatch::visit_reverse (m_visitors, msg_count, ctx);
        }

        // pre_/post_message visits:

        template<typename CTX>
        VR_FORCEINLINE bool visit (pre_message const msg_type, addr_const_t const msg, CTX & ctx) // override
        {
            bool const rc = (m_v_limit = vdispatch::visit_forward (m_visitors, msg_type, msg, ctx)); // return 'true' if at least one of'VISITORs' said 'true'
            assert_le (m_v_limit, vdispatch::depth ());

            return rc;
        }

        template<typename CTX>
        VR_FORCEINLINE void visit (post_message const msg_type, CTX & ctx) // override
        {
            assert_le (m_v_limit, vdispatch::depth ());

            vdispatch::visit_reverse (m_visitors, msg_type, ctx, m_v_limit);
        }

        // ITCH message visits:

#define vr_VISIT_MESSAGE(r, unused, MSG) \
        template<typename CTX> \
        VR_FORCEINLINE bool visit (itch:: MSG const & msg, CTX & ctx) /* override */ \
        { \
            bool const rc = (m_v_limit = vdispatch::visit_forward (m_visitors, msg, ctx, m_v_limit)); /* this can reduce 'm_v_limit' */ \
            assert_le (m_v_limit, vdispatch::depth ()); \
            \
            return rc; \
        } \
        /* */

        BOOST_PP_SEQ_FOR_EACH (vr_VISIT_MESSAGE, unused, VR_MARKET_ITCH_RECV_MESSAGE_SEQ)

#undef vr_VISIT_MESSAGE

    private: // ..............................................................

        uint32_t m_v_limit { }; // unsigned by design
        vtuple m_visitors;

}; // end of class

} // end of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
