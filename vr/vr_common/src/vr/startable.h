#pragma once

//----------------------------------------------------------------------------
namespace vr
{
/**
 * a lifecycle marker interface
 *
 * @see util::di::component
 * @see util::di::container
 */
class startable
{
    public: // ...............................................................

        virtual ~startable ()       = default;

        /**
         * invoked after construction but before any "business" API methods
         */
        virtual void start ()   { };

        /**
         * invoked prior to destruction and only if @ref start() has been invoked
         * and completed normally
         */
        virtual void stop ()    { };

}; // end of class

} // end of namespace
//----------------------------------------------------------------------------
