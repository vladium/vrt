#pragma once

#include "vr/market/sources.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
/**
 * @note this is a pretty generically parameterized template in order to be able
 * to friend all specialization by classes like 'execution_book', etc
 */
template<source::enum_t SOURCE, typename ...>
struct execution_listener; // master, specialized in source-specific headers

} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
