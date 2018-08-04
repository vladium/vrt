
#include "vr/fw_string.h"
#include "vr/market/books/execution_order_book.h"
#include "vr/market/sources.h"

#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
//............................................................................
//............................................................................
namespace test_
{

using otk_type              = fw_string8::storage_type;

struct src_traits: public source_traits<>
{
    using price_type                = int32_t;  // wire price
    using qty_type                  = int32_t;

    using iid_type                  = int32_t;
    using oid_type                  = int64_t;  // test smth different from 'qty_type'

    template<typename T_BOOK_PRICE>
    using price_traits              = price_ops<price_type, std::ratio<price_si_scale (), 10000>, T_BOOK_PRICE>;

}; // end of local class

} // end of 'test_'
//............................................................................
//............................................................................

// TODO
// - verify default capacities of pools

TEST (execution_order_book, traits)
{
    // defaults (a minimalistic book):
    {
        using book_type         = execution_order_book<double, test_::otk_type, test_::src_traits>;
        LOG_info << "sizeof {" << cn_<book_type> () << "} = " << sizeof (book_type);

        vr_static_assert (! book_type::has_user_data ());

        vr_static_assert (std::is_same<book_type::price_type, double>::value);

        vr_static_assert (std::is_same<book_type::oid_type, test_::src_traits::oid_type>::value);
        vr_static_assert (std::is_same<book_type::otk_type, test_::otk_type>::value);
        vr_static_assert (std::is_same<book_type::qty_type, test_::src_traits::qty_type>::value);

        // adding empty user_data does not increase the size:
        {
            using book_type2    = execution_order_book<double, test_::otk_type, test_::src_traits, user_data<>>;
            vr_static_assert (sizeof (book_type2) == sizeof (book_type));
        }
        // and 'user_data<void>' is another way of saying "none/empty":
        {
            using book_type2    = execution_order_book<double, test_::otk_type, test_::src_traits, user_data<void>>;
            vr_static_assert (sizeof (book_type2) == sizeof (book_type));
        }
    }

    // change book price type and add 'user_data':
    {
        struct custom_data
        {
            std::string m_name { };
            int32_t m_i32 { -1 };

        }; // end of local class

        using book_type         = execution_order_book<int32_t, test_::otk_type, test_::src_traits, user_data<custom_data>>;
        LOG_info << "sizeof {" << cn_<book_type> () << "} = " << sizeof (book_type);

        vr_static_assert (book_type::has_user_data ()); // ***
        vr_static_assert (std::is_same<custom_data, book_type::order_user_data>::value); // ***
        vr_static_assert (std::is_same<book_type::order_user_data, book_type::order_type::user_data_type>::value); // ***

        vr_static_assert (std::is_same<book_type::price_type, int32_t>::value); // ***

        vr_static_assert (std::is_same<book_type::oid_type, test_::src_traits::oid_type>::value);
        vr_static_assert (std::is_same<book_type::otk_type, test_::otk_type>::value);
        vr_static_assert (std::is_same<book_type::qty_type, test_::src_traits::qty_type>::value);
    }
}

} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
