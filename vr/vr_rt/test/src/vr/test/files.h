#pragma once

#include "vr/filesystem.h"
#include "vr/market/sources.h"
#include "vr/operators.h"
#include "vr/util/datetime.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace test
{
/**
 * this is currently only to support tests
 */
extern fs::path
find_capture (market::source::enum_t const src, rop::enum_t const op, util::date_t const & date);

} // end of 'test'
} // end of namespace
//----------------------------------------------------------------------------
