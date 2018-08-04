
#include "vr/market/books/limit_order_book.h"

#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
//............................................................................
// TODO
// - verify default capacities of pools (as derived from sizeof (level/object_type), etc)

TEST (limit_order_book, traits)
{
    using oid_type          = int32_t; // different integral type just to test

    // defaults (a minimalistic book):
    {
        using book_type         = limit_order_book<double, oid_type>;
        LOG_info << "sizeof {" << cn_<book_type> () << "} = " << sizeof (book_type);

        vr_static_assert (! book_type::const_time_depth ());
        vr_static_assert (! book_type::has_user_data ());

        vr_static_assert (std::is_same<book_type::price_type, double>::value);
        vr_static_assert (std::is_same<book_type::oid_type, oid_type>::value);

        vr_static_assert (book_type::order::has_qty ()); // currently always present
        vr_static_assert (! book_type::order::has_participant ());

        vr_static_assert (book_type::level::has_price ()); // currently always present
        vr_static_assert (! book_type::level::has_qty ());
        vr_static_assert (! book_type::level::has_order_count ());

        // adding empty user_data does not increase the size:
        {
            using book_type2    = limit_order_book<double, oid_type, user_data<>>;
            vr_static_assert (sizeof (book_type2) == sizeof (book_type));
        }
        // and 'user_data<void>' is another way of saying "none/empty":
        {
            using book_type2    = limit_order_book<double, oid_type, user_data<void>>;
            vr_static_assert (sizeof (book_type2) == sizeof (book_type));
        }
    }

    // add order participant field:
    {
        using book_type         = limit_order_book<price_si_t, oid_type, level<order <_participant_>>>;
        LOG_info << "sizeof {" << cn_<book_type> () << "} = " << sizeof (book_type);

        vr_static_assert (! book_type::const_time_depth ());

        vr_static_assert (std::is_same<book_type::price_type, price_si_t>::value);
        vr_static_assert (std::is_same<book_type::oid_type, oid_type>::value);

        vr_static_assert (book_type::order::has_qty ());
        vr_static_assert (book_type::order::has_participant ()); // ***

        vr_static_assert (book_type::level::has_price ());
        vr_static_assert (! book_type::level::has_qty ());
        vr_static_assert (! book_type::level::has_order_count ());
    }

    // add aggregate level qty:
    {
        using book_type         = limit_order_book<price_si_t, oid_type, level<_qty_>>;
        LOG_info << "sizeof {" << cn_<book_type> () << "} = " << sizeof (book_type);

        vr_static_assert (! book_type::const_time_depth ());

        vr_static_assert (std::is_same<book_type::price_type, price_si_t>::value);
        vr_static_assert (std::is_same<book_type::oid_type, oid_type>::value);

        vr_static_assert (book_type::order::has_qty ());
        vr_static_assert (! book_type::order::has_participant ());

        vr_static_assert (book_type::level::has_price ());
        vr_static_assert (book_type::level::has_qty ()); // ***
        vr_static_assert (! book_type::level::has_order_count ());


    }
    // add aggregate level order count:
    {
        using book_type         = limit_order_book<price_si_t, oid_type, level<_order_count_>>;
        LOG_info << "sizeof {" << cn_<book_type> () << "} = " << sizeof (book_type);

        vr_static_assert (! book_type::const_time_depth ());

        vr_static_assert (std::is_same<book_type::price_type, price_si_t>::value);
        vr_static_assert (std::is_same<book_type::oid_type, oid_type>::value);

        vr_static_assert (book_type::order::has_qty ());
        vr_static_assert (! book_type::order::has_participant ());

        vr_static_assert (book_type::level::has_price ());
        vr_static_assert (! book_type::level::has_qty ());
        vr_static_assert (book_type::level::has_order_count ()); // ***
    }

    // make side depth O(1):
    {
        using book_type         = limit_order_book<double, oid_type, _depth_>;
        LOG_info << "sizeof {" << cn_<book_type> () << "} = " << sizeof (book_type);

        vr_static_assert (book_type::const_time_depth ()); // ***

        vr_static_assert (std::is_same<book_type::price_type, double>::value);
        vr_static_assert (std::is_same<book_type::oid_type, oid_type>::value);

        vr_static_assert (book_type::order::has_qty ());
        vr_static_assert (! book_type::order::has_participant ());

        vr_static_assert (book_type::level::has_price ());
        vr_static_assert (! book_type::level::has_qty ());
        vr_static_assert (! book_type::level::has_order_count ());
    }


    // some combination of the above + 'user_data':
    {
        struct custom_data
        {
            std::string m_name { };
            int32_t m_i32 { -1 };

        }; // end of local class

        using book_type         = limit_order_book<int32_t, oid_type, user_data<custom_data>, _depth_, level<order<_participant_>, _qty_, _order_count_>>;
        LOG_info << "sizeof {" << cn_<book_type> () << "} = " << sizeof (book_type);

        vr_static_assert (book_type::const_time_depth ()); // ***
        vr_static_assert (book_type::has_user_data ()); // ***
        vr_static_assert (std::is_same<custom_data, book_type::book_user_data>::value); // ***

        vr_static_assert (std::is_same<book_type::price_type, int32_t>::value);
        vr_static_assert (std::is_same<book_type::oid_type, oid_type>::value);

        vr_static_assert (book_type::order::has_qty ());
        vr_static_assert (book_type::order::has_participant ()); // ***

        vr_static_assert (book_type::level::has_price ());
        vr_static_assert (book_type::level::has_qty ()); // ***
        vr_static_assert (book_type::level::has_order_count ()); // ***
    }
}

} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
