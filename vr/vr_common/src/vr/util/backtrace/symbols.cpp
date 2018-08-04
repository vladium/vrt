
#include "vr/util/backtrace/symbols.h"

#include "vr/asserts.h"
#include "vr/util/backtrace.h" // demangle()
#include "vr/util/logging.h"
#include "vr/util/singleton.h"

#include <memory>
#include <cstring>

#include <cxxabi.h> // abi::__cxa_demangle()
#include <dlfcn.h>
#include <elfutils/libdwfl.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

//----------------------------------------------------------------------------
namespace vr
{
namespace util
{
namespace backtrace
{
//............................................................................
//............................................................................
namespace
{
//............................................................................

static string_literal_t const g_unknown = "??";

// TODO
static char * g_default_debuginfo_path { nullptr };

//............................................................................

class DWARF_ifc final
{
    public: // ...............................................................

        DWARF_ifc () VR_NOEXCEPT // note: this may get invoked when constructing an exception, so don't throw
        {
            // set up 'm_callbacks':
            {
                m_callbacks.find_debuginfo = & ::dwfl_standard_find_debuginfo;
                m_callbacks.find_elf = & ::dwfl_linux_proc_find_elf;
                m_callbacks.debuginfo_path = & g_default_debuginfo_path;
            }

            m_dwfl = ::dwfl_begin (& m_callbacks);

            int32_t rc {};
            if ((rc = ::dwfl_linux_proc_report (m_dwfl, ::getpid ())))
            {
                m_init_failure = "dwfl_linux_proc_report() failure (rc "; m_init_failure += string_cast (rc); m_init_failure += ')';

                string_literal_t const dwflmsg = ::dwfl_errmsg (0);
                if (dwflmsg != nullptr)
                {
                    m_init_failure += ": ";
                    m_init_failure += dwflmsg;
                }
                else if (rc > 0) // try to interpret 'rc' as 'errno'
                {
                    m_init_failure += ": ";
                    m_init_failure += std::strerror (rc);
                }

                LOG_error << m_init_failure;
                return;
            }
            ::dwfl_report_end (m_dwfl, nullptr, nullptr);
        }

        ~DWARF_ifc () VR_NOEXCEPT
        {
            ::Dwfl * const ifc = m_dwfl;

            if (ifc)
            {
                m_dwfl = nullptr;
                ::dwfl_end (ifc);
            }
        }


        bool is_available () const
        {
            return m_init_failure.empty ();
        }

        const std::string & init_failure () const
        {
            return m_init_failure;
        }

        ::Dwfl * dwfl () const
        {
            return m_dwfl;
        }

    private: // ..............................................................

        ::Dwfl_Callbacks m_callbacks { }; // not sure if ::dwfl_begin() makes a deep copy, so maintain an instance
        ::Dwfl * m_dwfl { };
        std::string m_init_failure { }; // will be non-empty only if the constructor fails and the interface is not available

}; // end of class
//............................................................................

using DWARF_ifc_singleton           = util::singleton<DWARF_ifc>;

//............................................................................
/*
 * note: this handles null or empty 's'
 */
inline string_literal_t
stem_or_unknown (string_literal_t const s)
{
    if (s == nullptr) return g_unknown;

    if (VR_LIKELY (s [0]))
    {
        string_literal_t const last_sep = std::strrchr (s, '/');

        if (VR_LIKELY ((last_sep != nullptr) && last_sep [1])) // '/' present and not the last char in 's'
            return (last_sep + 1);
        else
            return s;
    }

    return g_unknown;
}
//............................................................................

void
emit_info (string_literal_t const mod_name, string_literal_t const sym_name, string_literal_t const src, int32_t line, std::ostream & out, addr_t const addr)
{
    // TODO format option for showing abs pathname:
    out << '[' << stem_or_unknown (mod_name);

    if ((src != nullptr) && src [0])
    {
        // TODO format option for showing abs pathname:
        out << "; " << stem_or_unknown (src) << ':';

        if (line > 0)
            out << line;
        else
            out << g_unknown;
    }

    out << "]\t";

    if ((sym_name != nullptr) && sym_name [0])
        out << demangle (sym_name);
    else
        out << addr;
}
//............................................................................

void
default_print_addr (addr_t const addr, std::ostream & out, std::string const & prefix)
{
    assert_nonnull (addr);

    out << prefix;

    // note: backtrace_symbols() from <execinfo.h> uses ::dladdr() and hence can't
    // provide richer information that what we do here:
    {
        ::Dl_info dli { };

        if (VR_LIKELY (::dladdr (addr, & dli))) // success:
        {
            emit_info (dli.dli_fname, dli.dli_sname, nullptr, 0, out, addr);
        }
        else
        {
            out << addr;
        }
    }

    out << "\r\n";
}
//............................................................................

void
default_print_trace (trace_data const & trace, std::ostream & out, std::string const & prefix)
{
    for (int32_t d = 0; d < trace.m_size; ++ d)
    {
        addr_t const addr = trace.m_addr [d];
        assert_nonnull (addr);

        default_print_addr (addr, out, prefix);
    }
}

} // end of anonymous
//............................................................................
//............................................................................

std::string
demangle (string_literal_t const mangled_name)
{
    int32_t rc { 1 };

    std::unique_ptr<char, free_fn_t> const str { ::abi::__cxa_demangle (mangled_name, nullptr, nullptr, & rc), std::free };

    return { rc ? mangled_name : str.get () };
}
//............................................................................

void
print_trace (trace_data const & trace, std::ostream & out, std::string const & prefix)
{
    assert_positive (trace.m_size); // caller guarantee

    DWARF_ifc & ifc = DWARF_ifc_singleton::instance ();

    if (VR_UNLIKELY (! ifc.is_available ()))
    {
        default_print_trace (trace, out, prefix);
        return;
    }

    ::Dwfl * const dwfl = ifc.dwfl ();
    assert_nonnull (dwfl);

    for (int32_t d = 0; d < trace.m_size; ++ d)
    {
        addr_t const addr = trace.m_addr [d];
        assert_nonnull (addr);

        //format_addr (trace.m_addr [d], out, prefix);

        ::GElf_Addr elf_addr = reinterpret_cast<std_uintptr_t> (addr);
        ::Dwfl_Module * const module = ::dwfl_addrmodule (dwfl, elf_addr);

        if (VR_UNLIKELY (module == nullptr))
        {
            LOG_error << "could not find ELF module for " << addr;

            default_print_addr (addr, out, prefix);
            continue;
        }

        out << prefix;

        // TODO memoize this:
        // TODO make use of MAINFILE and/or DEBUGFILE

        string_literal_t const mod_name = ::dwfl_module_info (module, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);

        ::GElf_Sym sym { };
        ::GElf_Word shndx { };
        // TODO dwfl_module_addrinfo() recommended instead:
        string_literal_t const sym_name = ::dwfl_module_addrsym (module, elf_addr, & sym, & shndx);

        string_literal_t src { nullptr };
        int32_t lineno { };

        ::Dwfl_Line * const line = ::dwfl_module_getsrc (module, elf_addr);
        if (line != nullptr)
        {
            src = ::dwfl_lineinfo (line, nullptr, & lineno, nullptr, nullptr, nullptr);
        }

        // out << prefix << demangle (name) << " + 0x" << std::hex << (elf_addr - sym.st_value) << " (" << modname << ')' << std::endl;

        emit_info (mod_name, sym_name, src, lineno, out, addr);

        out << "\r\n";
    }
}

} // end of 'backtrace'
} // end of 'util'
} // end of namespace
//----------------------------------------------------------------------------
