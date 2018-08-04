
#include "vr/sys/signals.h"

#include "vr/enums.h"
#include "vr/io/streams.h"
#include "vr/sys/os.h"
#include "vr/util/backtrace.h"
#include "vr/util/backtrace/symbols.h"
#include "vr/util/env.h"
#include "vr/util/logging.h"
#include "vr/util/ops_int.h"
#include "vr/util/parse.h"
#include "vr/util/singleton.h"

#include <atomic>
#include <iostream>

#include <signal.h>
#include <unistd.h> // sleep(), STDOUT_FILENO

//----------------------------------------------------------------------------
namespace vr
{
namespace sys
{
//............................................................................

#define vr_ENV_SIG_BACKTRACE_PREFIX     "  "
#define vr_ENV_SIG_DEFAULT              "SIGUSR2:backtrace; SIGSEGV,SIGILL,SIGFPE,SIGABRT,SIGBUS:fataltrace" // note: leaving SIGUSR1 for urcu default

//............................................................................

static std::atomic<int32_t> g_fatal_counter { };
static std::atomic<int32_t> g_fatal_exit_flag { };

//............................................................................
//............................................................................
namespace
{

VR_EXPLICIT_ENUM (signum,
    int32_t,
        (SIGHUP,    SIGHUP_)    // note: appending '_' to form enum val names
        (SIGINT,    SIGINT_)
        (SIGQUIT,   SIGQUIT_)
        (SIGILL,    SIGILL_)
        (SIGTRAP,   SIGTRAP_)
        (SIGABRT,   SIGABRT_)
        (SIGBUS,    SIGBUS_)
        (SIGFPE,    SIGFPE_)
        (SIGKILL,   SIGKILL_)
        (SIGUSR1,   SIGUSR1_)
        (SIGSEGV,   SIGSEGV_)
        (SIGUSR2,   SIGUSR2_)
        (SIGPIPE,   SIGPIPE_)
        (SIGALRM,   SIGALRM_)
        (SIGTERM,   SIGTERM_)
        (SIGSTKFLT, SIGSTKFLT_)
        (SIGCHLD,   SIGCHLD_)
        (SIGCONT,   SIGCONT_)
        (SIGSTOP,   SIGSTOP_)
        (SIGTSTP,   SIGTSTP_)
        (SIGTTIN,   SIGTTIN_)
        (SIGTTOU,   SIGTTOU_)
        (SIGURG,    SIGURG_)
        (SIGXCPU,   SIGXCPU_)
        (SIGXFSZ,   SIGXFSZ_)
        (SIGVTALRM, SIGVTALRM_)
        (SIGPROF,   SIGPROF_)
        (SIGWINCH,  SIGWINCH_)
        (SIGIO,     SIGIO_)
        (SIGPWR,    SIGPWR_)
        (SIGSYS,    SIGSYS_)
    ,
    printable, parsable // to parse, append '_' TODO can also use strsignal() instead
);

/*
 *
 */
inline string_literal_t
si_code_name (int32_t const sig, int32_t const si_code)
{
#define vr_CASE(r, unused, code) case code : return VR_TO_STRING (code) " ";

    switch (sig)
    {
        case SIGILL:
        {
            switch (si_code)
            {
#           define vr_SI_CODE_SEQ   \
                (ILL_ILLOPC)        \
                (ILL_ILLOPN)        \
                (ILL_ILLADR)        \
                (ILL_ILLTRP)        \
                (ILL_PRVOPC)        \
                (ILL_PRVREG)        \
                (ILL_COPROC)        \
                (ILL_BADSTK)        \
                /* */

                BOOST_PP_SEQ_FOR_EACH (vr_CASE, unused, vr_SI_CODE_SEQ)

#           undef vr_SI_CODE_SEQ
            } // end of switch
        }
        break;

        case SIGFPE:
        {
            switch (si_code)
            {
#           define vr_SI_CODE_SEQ   \
                (FPE_INTDIV)        \
                (FPE_INTOVF)        \
                (FPE_FLTDIV)        \
                (FPE_FLTOVF)        \
                (FPE_FLTUND)        \
                (FPE_FLTRES)        \
                (FPE_FLTINV)        \
                (FPE_FLTSUB)        \
                /* */

                BOOST_PP_SEQ_FOR_EACH (vr_CASE, unused, vr_SI_CODE_SEQ)

#           undef vr_SI_CODE_SEQ
            } // end of switch
        }
        break;

        case SIGSEGV:
        {
            switch (si_code)
            {
#           define vr_SI_CODE_SEQ   \
                (SEGV_MAPERR)       \
                (SEGV_ACCERR)       \
                /* */

                BOOST_PP_SEQ_FOR_EACH (vr_CASE, unused, vr_SI_CODE_SEQ)

#           undef vr_SI_CODE_SEQ
            } // end of switch
        }
        break;

        case SIGBUS:
        {
            switch (si_code)
            {
#           define vr_SI_CODE_SEQ   \
                (BUS_ADRALN)        \
                (BUS_ADRERR)        \
                (BUS_OBJERR)        \
                /* */

                BOOST_PP_SEQ_FOR_EACH (vr_CASE, unused, vr_SI_CODE_SEQ)

#           undef vr_SI_CODE_SEQ
            } // end of switch
        }
        break;

    } // end of switch

#undef vr_CASE

    return nullptr;
}

} // end of anonymous
//............................................................................
//............................................................................

static void // note: do not put inside 'anonymous'
sigact_backtrace (int32_t const sig, ::siginfo_t * const siginfo, /* ucontext_t */addr_t const ucontext)
{
    // TODO process all process' threads instead of just the one that gets the signal (NOT EASY)

    // TODO make use of extra info in 'ucontext' (PC, etc)

    int32_t out_written { };
    int8_t out_buf [4 * 1024]; // TODO better to use sigaltstack
    {
        constexpr int32_t skip              = 2;

        trace_data trace { };
        trace.m_size = util::capture_callstack (skip, trace.m_addr, trace_data::depth_limit); // probably not async-safe

        io::array_ostream<io::no_exceptions> out { out_buf };
        util::backtrace::print_trace (trace, out, vr_ENV_SIG_BACKTRACE_PREFIX); // probably not async-safe

        out_written = out.tellp ();
    }
    ::write (STDOUT_FILENO, out_buf, out_written); // async-safe

    // TODO invoke rest of handler chain?
}

static void // note: do not put inside 'anonymous'
sigact_fataltrace (int32_t const sig, ::siginfo_t * const siginfo, /* ucontext_t */addr_t const ucontext)
{
    if (++ g_fatal_counter == 1) // first thread to enter
    {
        // TODO make use of extra info in 'ucontext' (PC, etc)

        constexpr int32_t skip              = 3;

        int32_t out_written { };
        int8_t out_buf [4 * 1024]; // TODO better to use sigaltstack
        {
            trace_data trace { };
            trace.m_size = util::capture_callstack (skip, trace.m_addr, trace_data::depth_limit); // probably not async-safe

            io::array_ostream<io::no_exceptions> out { out_buf };

            out << '\n' << static_cast<signum::enum_t> (sig)
                << '(' << (si_code_name (sig, siginfo->si_code) ?: "") << "@ " << static_cast<addr_const_t> (siginfo->si_addr) << ") received by PID " << ::getpid ()
                << " (TID " << sys::gettid () << ", pthread_t 0x" << std::hex << sys::pthreadid () << std::dec
                << ") from uid " << siginfo->si_uid << ", PID " << siginfo->si_pid << "\n\n";

            util::backtrace::print_trace (trace, out, vr_ENV_SIG_BACKTRACE_PREFIX); // probably not async-safe

            out_written = out.tellp ();
        }
        ::write (STDOUT_FILENO, out_buf, out_written); // async-safe

        ++ g_fatal_exit_flag;
    }
    else // another thread  already performing this action
    {
        for (int32_t retry = 0; (retry < 5) && ! g_fatal_exit_flag; ++ retry) // wait for it to finish and then attempt to longjmp to a "safe" point
        {
            ::sleep (1); // async-safe
        }

        // TODO ::siglongjmp()
        // TODO flush tracing if applicable
    }

    ::_exit (128 + sig);
}
//............................................................................
//............................................................................
namespace
{

VR_ENUM (sigact,
    (
        backtrace,
        fataltrace
    )
    ,
    printable, parsable

); // end of enum
//............................................................................

typedef struct ::sigaction  sa_t;

using bitset_t              = bitset64_t;
vr_static_assert (sizeof (bitset_t) * 8 >= NSIG - 1);

//............................................................................

struct signal_setup final: noncopyable
{
    VR_ASSUME_COLD signal_setup (); // impl below to solve some circular def cases
    VR_ASSUME_COLD ~signal_setup () VR_NOEXCEPT; // calls 'cleanup()'

    VR_ASSUME_COLD void cleanup () VR_NOEXCEPT; // idempotent


    bitset_t m_sig_setup_done { }; // one at position 'i' indicates that signum (i + 1) has been initialized
    sa_t m_sa_prev [(NSIG - 2)];

}; // end of class
//............................................................................

template<sigact::enum_t SA>
struct sigact_impl; // end of master

// specializations:

template<>
struct sigact_impl<sigact::backtrace>
{
    static bool evaluate (std::vector<signum::enum_t> const & signums, signal_setup & setup_obj)
    {
        sa_t sa;
        std::memset (& sa, 0, sizeof (sa));
        {
            sa.sa_sigaction = & sigact_backtrace;
            ::sigemptyset (& sa.sa_mask);
            sa.sa_flags |= (SA_RESTART | SA_SIGINFO);
        }

        for (signum::enum_t const sig : signums)
        {
            LOG_trace2 << "  setting up " << sig << " ...";
            bitset_t const sig_bit = (1L << (sig - 1));

            if (VR_UNLIKELY (setup_obj.m_sig_setup_done & sig_bit))
            {
                LOG_error << "  " << sig << " appears elsewhere in this setup, aborting ...";
                return false; // caller will undo dispositions that've succeeded earlier
            }

            if (VR_CHECKED_SYS_CALL_noexcept (::sigaction (sig, & sa, & setup_obj.m_sa_prev [sig - 1])) < 0)
                return false; // caller will undo dispositions that've succeeded earlier

            setup_obj.m_sig_setup_done |= sig_bit;
        }

        return true;
    }

}; // end of specialization

template<>
struct sigact_impl<sigact::fataltrace>
{
    static bool evaluate (std::vector<signum::enum_t> const & signums, signal_setup & setup_obj)
    {
        sa_t sa;
        std::memset (& sa, 0, sizeof (sa));
        {
            sa.sa_sigaction = & sigact_fataltrace;
            ::sigemptyset (& sa.sa_mask);
            sa.sa_flags |= (SA_RESTART | SA_SIGINFO); // TODO SA_ONSTACK impl
        }

        for (signum::enum_t const sig : signums)
        {
            LOG_trace2 << "  setting up " << sig << " ...";
            bitset_t const sig_bit = (1L << (sig - 1));

            if (VR_UNLIKELY (setup_obj.m_sig_setup_done & sig_bit))
            {
                LOG_error << "  " << sig << " appears elsewhere in this setup, aborting ...";
                return false; // caller will undo dispositions that've succeeded earlier
            }

            if (VR_CHECKED_SYS_CALL_noexcept (::sigaction (sig, & sa, & setup_obj.m_sa_prev [sig - 1])) < 0)
                return false; // caller will undo dispositions that've succeeded earlier

            setup_obj.m_sig_setup_done |= sig_bit;
        }

        return true;
    }

}; // end of specialization
//............................................................................

struct sig_setup_cfg final
{
    std::vector<signum::enum_t> m_signums;
    sigact::enum_t m_action;

}; // end of class

/*
 * note: a signal isn't allowed to have more than one 'sigact' in the current impl
 */
VR_ASSUME_COLD bool
parse_sig_setup_spec (std::string const & spec, std::vector<sig_setup_cfg> & out)
{
    string_vector const tokens = util::split (spec, "; ");

    for (std::string const & tok : tokens)
    {
        string_vector const nv = util::split (tok, ":");
        if (signed_cast (nv.size ()) != 2)
        {
            LOG_error << "invalid signal setup token " << print (tok);
            return false;
        }

        out.emplace_back ();
        sig_setup_cfg & ssc = out.back ();

        string_vector const signames = util::split (nv [0], ",");
        if (signames.empty ())
        {
            LOG_error << "no signal name(s) in setup token " << print (tok);
            return false;
        }

        try
        {
            ssc.m_action = to_enum<sigact> (nv [1]);

            for (std::string const & signame : signames)
            {
                ssc.m_signums.push_back (to_enum<signum> (signame + '_'));
            }
        }
        catch (std::exception const & e)
        {
            LOG_error << "failed to parse signal token " + print (tok) + ": " + e.what ();
            return false;
        }

        LOG_trace1 << "signal setup [" << print (ssc.m_signums) << " -> " << print (ssc.m_action) << ']';
    }

    return true;
}
//............................................................................
//............................................................................

VR_ASSUME_COLD
signal_setup::signal_setup ()
{
    std::string const spec { util::getenv<std::string> (VR_ENV_SIG, vr_ENV_SIG_DEFAULT) };

    std::vector<sig_setup_cfg> sscs { };
    if (! parse_sig_setup_spec (spec, sscs))
        return;

    bool rc { false };
    for (auto const & ssc : sscs)
    {
        switch (ssc.m_action)
        {
            case sigact::backtrace:     rc = sigact_impl<sigact::backtrace>::evaluate (ssc.m_signums, * this); break;
            case sigact::fataltrace:    rc = sigact_impl<sigact::fataltrace>::evaluate (ssc.m_signums, * this); break;

            default: VR_ASSUME_UNREACHABLE (ssc.m_action);

        } // end of switch

        if (! rc) break;
    }

    if (! rc) // TODO abort current process?
    {
        LOG_error << "some signal setup failures, restoring dispositions ...";
        cleanup ();
    }
}

VR_ASSUME_COLD
signal_setup::~signal_setup () VR_NOEXCEPT
{
    cleanup ();
}
//............................................................................

VR_ASSUME_COLD void
signal_setup::cleanup () VR_NOEXCEPT // idempotent
{
    using bit_ops       = util::ops_int<util::arg_policy<util::zero_arg_policy::ignore, 0>, true>; // checked

    bitset_t sig_setup_set = m_sig_setup_done;
    m_sig_setup_done = { };

    while (sig_setup_set)
    {
        bitset_t const sig_bit = sig_setup_set & (- sig_setup_set);
        signum::enum_t const sig = static_cast<signum::enum_t> (bit_ops::log2_floor (sig_bit) + 1);

        LOG_trace2 << "  restoring " << sig << " ...";

        VR_CHECKED_SYS_CALL_noexcept (::sigaction (sig, & m_sa_prev [sig - 1], nullptr));

        sig_setup_set &= ~ sig_bit;
    }
}

} // end of anonymous
//............................................................................
//............................................................................

bool
setup_signal_handlers ()
{
    return util::singleton<signal_setup>::instance ().m_sig_setup_done;
}

} // end of 'sys'
} // end of namespace
//----------------------------------------------------------------------------
