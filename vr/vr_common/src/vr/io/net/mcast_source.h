#pragma once

#include "vr/io/net/defs.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
namespace net
{
/**
 */
class mcast_source final
{
    public: // ...............................................................

        /**
         * @param spec string in the form 'src_ip->(grp_ip[:grp_port])+'
         */
        mcast_source (std::string const & spec);

        // ACCESSORs:

        std::string const & source () const
        {
            return m_source;
        }

        /**
         * @return [never empty]
         */
        std::vector<addr_and_port> const & groups () const
        {
            return m_groups;
        }

        std::string native () const;

        // FRIENDs:

        friend VR_ASSUME_COLD std::string __print__ (mcast_source const & obj) VR_NOEXCEPT
        {
            return '[' + obj.native () + ']';
        }

        friend std::ostream & operator<< (std::ostream & os, mcast_source const & obj) VR_NOEXCEPT
        {
            return os << obj.native ();
        }

    private: // ..............................................................

        std::string m_source { };
        std::vector<addr_and_port> m_groups { };

}; // end of class

} // end of 'net'
} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
