#pragma once

#include "vr/util/di/component_impl.h"
#include "vr/util/memory.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
namespace di
{
class container;

/**
 */
class component: component_impl // inherit some impl details privately to keep the interface header uncluttered
{
    public: // ...............................................................

        virtual ~component ()   = default;

    protected: // ............................................................

        /**
         * dependency injection helper, to be used as in
         * @code
         *     class my_component: public component
         *     {
         *         my_component ()
         *         {
         *             dep (m_api) = "API_impl_name";
         *         }
         *         ...
         *         API * m_api { };
         *     };
         * @endcode
         *
         * helper to associate a dependency field with its implementation
         * name (within the parent container)
         */
        template<typename T>
        dep_proxy dep (T * & dep_field)
        {
            return { dep_field, * m_resolve_context };
        }

    private: // ..............................................................

        friend class container;

        std::unique_ptr<dep_context> m_resolve_context { std::make_unique<dep_context> () }; // note: discarded after container init

}; // end of class

} // end of 'di'
} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
