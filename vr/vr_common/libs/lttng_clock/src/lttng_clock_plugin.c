
#include "vr/rt/tracing/defs.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <stdlib.h>
#include <time.h>

#include <lttng/ust-clock.h>

//----------------------------------------------------------------------------

#define VR_ASSUME_HOT               __attribute__ ((hot))
#define VR_ASSUME_COLD              __attribute__ ((cold))

//............................................................................

#define _1_SEC  (1000000000UL)

static VR_ASSUME_HOT uint64_t
vr_clock_time ()
{
    struct timespec t;
    clock_gettime (CLOCK_REALTIME, & t);

    return (t.tv_sec * _1_SEC + t.tv_nsec - 5 * 3600 * _1_SEC); // TODO workaround for https://bugs.lttng.org/issues/1168
}
//............................................................................
/*
 * not sure exactly how this is used and whether it needs to be "hot"; perusing
 * LTTng sources shows that this value is cached internally and the fn may be
 * called just once during init
 */
static VR_ASSUME_HOT uint64_t
vr_clock_freq ()
{
    return _1_SEC;
}
//............................................................................

static VR_ASSUME_COLD int32_t
vr_clock_plugin_uuid (char * const uuid)
{
    strncpy (uuid, VR_CLOCK_PLUGIN_UUID, LTTNG_UST_UUID_STR_LEN);

    return 0;
}

static VR_ASSUME_COLD const char *
vr_clock_plugin_name ()
{
    return "vr_rt_clock";
}

static VR_ASSUME_COLD const char *
vr_clock_plugin_description ()
{
    return "a clock that replicates CLOCK_REALTIME";
}
//............................................................................

VR_ASSUME_COLD void
lttng_ust_clock_plugin_init ()
{
    fprintf (stdout, "\n'%s' init: entry\n", vr_clock_plugin_name ());

#define vr_CHECKED_LTTNG_TRACE_CLOCK_CALL_noexcept(exp) \
    { \
       int32_t const rc = (exp); \
       if (rc) { fprintf (stderr, "(" #exp ") error (%d): %s\n", rc, strerror (- rc)); goto error_exit; }; \
    } \
    /* */

    vr_CHECKED_LTTNG_TRACE_CLOCK_CALL_noexcept (lttng_ust_trace_clock_set_read64_cb (vr_clock_time));
    vr_CHECKED_LTTNG_TRACE_CLOCK_CALL_noexcept (lttng_ust_trace_clock_set_freq_cb (vr_clock_freq));
    vr_CHECKED_LTTNG_TRACE_CLOCK_CALL_noexcept (lttng_ust_trace_clock_set_uuid_cb (vr_clock_plugin_uuid));
    vr_CHECKED_LTTNG_TRACE_CLOCK_CALL_noexcept (lttng_ust_trace_clock_set_name_cb (vr_clock_plugin_name));
    vr_CHECKED_LTTNG_TRACE_CLOCK_CALL_noexcept (lttng_ust_trace_clock_set_description_cb (vr_clock_plugin_description));

    vr_CHECKED_LTTNG_TRACE_CLOCK_CALL_noexcept (lttng_ust_enable_trace_clock_override ());

#undef vr_CHECKED_LTTNG_TRACE_CLOCK_CALL_noexcept

    fprintf (stdout, "'%s' init: ok\n", vr_clock_plugin_name ());
    return;

error_exit:

    fprintf (stderr, "'%s' init FAILED\n", vr_clock_plugin_name ());
    exit (EXIT_FAILURE);
}
//----------------------------------------------------------------------------
