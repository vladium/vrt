#pragma once

#include "vr/types.h"

#include <condition_variable>
#include <mutex>

//----------------------------------------------------------------------------
namespace vr
{
namespace mc
{
/**
 * @note even though this uses 'timestamp_t' (nanosecond) data type, the intended
 *       usage if for coarse timing
 *
 * @see util::di::container
 */
class waitable final
{
    public: // ...............................................................

        // ACCESSORs:

        bool is_signalled () const;

        // MUTATORs:

        void wait ();
        /**
         * @return 'true' if this waitable has been signalled, 'false' otherwise
         */
        bool wait_for (timestamp_t const duration);
        /**
         * @return 'true' if this waitable has been signalled, 'false' otherwise
         */
        bool wait_until (timestamp_t const time_utc);

        void signal ();

    private: // ..............................................................

        mutable std::mutex m_cv_mutex { };
        std::condition_variable m_cv { };
        bool m_signalled { false };

}; // end of class

} // end of 'mc'
} // end of namespace
//----------------------------------------------------------------------------
