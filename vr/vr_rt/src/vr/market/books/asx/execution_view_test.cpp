
#include "vr/market/books/asx/execution_view.h"
#include "vr/market/books/asx/execution_listener.h"
#include "vr/market/books/execution_order_book.h"

#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
namespace ASX
{
//............................................................................

//TEST (ASX_execution_view_test, create)
//{
//    using book_type         = execution_order_book<price_si_t, order_token, this_source_traits>;
//    using view              = execution_view<book_type>;
//
//    view exv { };
//
//    using visit_ctx         = meta::empty_struct; // TODO
//    using listener          = execution_listener<this_source (), book_type, visit_ctx>;
//
//    // TODO this isn't testing much beyond being able to compile/instantiate a view and a listener;
//    // need to test against a stream of msgs (fake using mock_tool)
//}

} // end of 'ASX
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
