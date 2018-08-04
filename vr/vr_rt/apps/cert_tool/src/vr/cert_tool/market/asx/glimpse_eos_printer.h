#pragma once

#include "vr/market/sources/asx/market_data.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
namespace ASX
{
/**
 */
template<typename CTX>
struct glimpse_eos_printer final: public ITCH_visitor<glimpse_eos_printer<CTX>>
{
    using super         = ITCH_visitor<glimpse_eos_printer<CTX>>;


    glimpse_eos_printer (arg_map const & args) :
        super (args)
    {
    }


    // overridden visits:

    using super::visit; // this shouldn't be necessary...


    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // Glimpse ITCH, server->client:
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    VR_ASSUME_HOT bool visit (itch::end_of_snapshot const & msg, CTX & ctx) // override
    {
        std::string p_prefix { };

#   define vr_EOL  "\r\n"

        if (has_field<_partition_, CTX> ())
        {
            auto const & p = field<_partition_> (ctx);

            p_prefix = "[P" + string_cast (p) + "]";
        }

        std::string notice;
        {
            std::stringstream ss { };

            ss << "-----------------------------------------------" vr_EOL
               << p_prefix << " END OF SNAPSHOT " << msg << vr_EOL
               << "-----------------------------------------------" vr_EOL;

            notice = ss.str ();
        }

        std::cout << notice;
        LOG_trace1 << notice;

#   undef vr_EOL

        return super::visit (msg, ctx); // chain
    }

}; // end of class

} // end of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
