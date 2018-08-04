#pragma once

#include "vr/asserts.h"
#include "vr/preprocessor.h"

#undef  BOOST_INTRUSIVE_SAFE_HOOK_DESTRUCTOR_ASSERT_INCLUDE
#define BOOST_INTRUSIVE_SAFE_HOOK_DESTRUCTOR_ASSERT_INCLUDE "vr/asserts.h"
#undef  BOOST_INTRUSIVE_SAFE_HOOK_DESTRUCTOR_ASSERT
#define BOOST_INTRUSIVE_SAFE_HOOK_DESTRUCTOR_ASSERT         assert_condition

#include <boost/intrusive/list.hpp>
#include <boost/intrusive/slist.hpp>

//----------------------------------------------------------------------------
namespace vr
{
namespace intrusive
{
namespace bi    = boost::intrusive;

struct bi_traits
{
    // 'safe_link' adds a lot of overhead, so use in debug builds only:

    static constexpr bool safe_mode ()  { return VR_IF_THEN_ELSE (VR_DEBUG)(true, false); }
    using link_mode                     = bi::link_mode<VR_IF_THEN_ELSE (VR_DEBUG)(bi::safe_link, bi::normal_link)>;

}; // end of metafunction

} // end of 'intrusive'
} // end of namespace
//----------------------------------------------------------------------------
