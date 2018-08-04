#pragma once

#include "vr/containers/util/stable_vector.h"
#include "vr/enums.h"
#include "vr/market/sources/asx/defs.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
namespace ASX
{
//............................................................................

struct order_descriptor final
{
    order_descriptor (order_token const otk, int32_t const partition) :
        m_otk_current { otk },
        m_partition { partition }
    {
    }

    VR_NESTED_ENUM (
        (
            submitted,
            live,
            rejected,
            done
        ),
        printable, parsable

    ) // end of enum

    friend std::ostream & operator<< (std::ostream & os, order_descriptor const & obj) VR_NOEXCEPT;

    order_token m_otk_current;
    int64_t m_qty_executed { };
    int32_t const m_partition;
    enum_t m_state { submitted };
    int32_t m_reject_code { };
    cancel_reason::enum_t m_cancel_code { cancel_reason::na };

}; // end of class
//............................................................................
/**
 * this is useless for anything but cert practice
 */
class ouch_order_book final: noncopyable
{
    public: // ...............................................................

        ouch_order_book ()  = default;


        // ACCESSORs:

        int32_t list (std::ostream & out) const;

        // MUTATORs:

        order_descriptor & add (order_token const otk, int32_t const partition);

        order_descriptor & add_alias (order_token const otk, order_token const new_otk);
        order_descriptor & lookup (order_token const otk, bool const add_if_missing = false);

    private: // ..............................................................

        using od_vector     = util::stable_vector<order_descriptor>;
        using otk_map       = boost::unordered_map<order_token, order_descriptor *>;


        od_vector m_odv { };    // stable
        otk_map m_otk_map { };  // points to 'm_odv' slots

}; // end of class

} // and of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
