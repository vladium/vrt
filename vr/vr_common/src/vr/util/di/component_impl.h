#pragma once

#include "vr/asserts.h"

#include <tuple>

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
namespace di
{
class component; // forward

/**
 */
class component_impl
{
    public: // ...............................................................

        template<typename T, typename U>
        static void dynamic_caster (T * & lhs_field, U * const rhs) // TODO where to keep this?
        {
            // TODO const correctness

            check_null (lhs_field); // confirm idempotency

            T * const lhs = dynamic_cast<T *> (rhs);
            if (! lhs)
                throw_x (type_mismatch, "instance of {" + util::class_name (* rhs) + "} can't be cast to {" + util::class_name<T> () + '}');

            lhs_field = lhs;
        }

        using bind_closure      = void (*) (addr_t, component *);

        struct dep_context final
        {
            using dep_map   = boost::unordered_map<addr_t, std::tuple<std::string, bind_closure> >;

            dep_map m_dep_map;

        }; // end of nested class

        class dep_proxy final
        {
            friend class component;

            // TODO preserve const info

            template<typename T>
            dep_proxy (T * & dep_field, dep_context & ctx) :
                m_dep_field { & dep_field },
                m_bind_closure { reinterpret_cast<bind_closure> (dynamic_caster<T, component>) },
                m_ctx { & ctx }
            {
            }


            /* T * & */addr_t const m_dep_field;
            bind_closure const m_bind_closure;
            dep_context * const m_ctx;

            public:

                void operator= (std::string const & name) const;

        }; // end of nested class

}; // end of class

} // end of 'di'
} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
