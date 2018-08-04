#pragma once

#include "vr/macros.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace sys
{

extern bool
setup_signal_handlers ();

} // end of 'sys'
} // end of namespace
//............................................................................
//............................................................................
namespace
{
/*
 * trigger signal initialization from any compilation unit that includes this header
 * (the actual setup logic will be executed once only).
 */
bool const vr_signal_setup_done VR_UNUSED { vr::sys::setup_signal_handlers () };

} // end of anonymous
//----------------------------------------------------------------------------
