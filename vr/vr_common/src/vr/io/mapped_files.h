#pragma once

#include "vr/asserts.h"
#include "vr/filesystem.h"
#include "vr/io/defs.h"
#include "vr/util/logging.h"

#include <sys/mman.h> // PROT_*

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
//............................................................................
/**
 * wrapper around ::mmap() with nicer error handling
 */
extern addr_t
mmap_fd (addr_t const addr, signed_size_t const extent, bitset32_t const prot, bitset32_t const flags,
         int32_t const fd, signed_size_t const offset);

//............................................................................
//............................................................................
namespace impl
{

class mapped_file // TODO make movable
{
    public: // ...............................................................

        // ACCESSORs:

        /*
         * for ifile, fixed at existing file's construction time;
         *
         * for ofile, starts at zero and denotes the offset that was in effect when the file is close()d (explicitly
         *            or by destructor) -- this is the final size the file will be truncated to
         */
        pos_t const & size () const
        {
            return m_size;
        }

        bool eof () const
        {
            return (m_fd < 0);
        }

    protected: // ............................................................

        mapped_file (advice_dir::enum_t const advice);
        ~mapped_file () VR_NOEXCEPT; // calls 'close ()'

        void close () VR_NOEXCEPT; // idempotent

        void advise () VR_NOEXCEPT_IF (VR_RELEASE);

        VR_ASSUME_HOT addr_t reposition (pos_t const position, window_t const window, bitset32_t const prot); // not inlined by design

        template<bitset32_t PROT, bool CHECK_INPUT = VR_CHECK_INPUT>
        addr_const_t
        seek_impl (pos_t const position, window_t const window)
        {
            if (CHECK_INPUT) check_condition ((position >= 0) & (window > 0), position, window);

            window_t const begin    = position - m_base_position;
            window_t const end      = begin + window;

            // fast path (no remapping):

            if (VR_LIKELY (vr_is_within (begin, m_extent) & vr_is_within_inclusive (end, m_extent)))
                return static_cast<addr_const_t> (m_base_addr + (m_offset = begin)); // note: 'm_offset side effect
            else
            {
                // otherwise, need to remap (and possibly increase extent):

                return reposition (position, window, PROT); // TODO 'ifile' should check for EOF
            }
        }


        pos_t m_size { };
        int8_t * m_base_addr { nullptr }; // invariant: if not null, always of 'm_extent' size
        pos_t m_base_position { };
        window_t m_offset { };
        window_t m_extent { }; // zero at construction, positive after any I/O, invariant: 'm_extent % sys::instance ().page_size () == 0'
        int32_t m_fd { -1 };
        advice_dir::enum_t const m_advice;

}; // end of class

} // end of 'impl'
//............................................................................
//............................................................................

class mapped_ifile final: public impl::mapped_file
{
    private: // ..............................................................

        using super                 = impl::mapped_file;

    public: // ...............................................................

        mapped_ifile (fs::path const & file, advice_dir::enum_t const advice = advice_dir::forward);


        // MUTATORs:

        template<bool CHECK_INPUT = VR_CHECK_INPUT>
        addr_const_t
        advance (pos_t const step, window_t const window);

        template<bool CHECK_INPUT = VR_CHECK_INPUT>
        addr_const_t
        seek (pos_t const position, window_t const window);


}; // end of class
//............................................................................

class mapped_ofile final: public impl::mapped_file
{
    private: // ..............................................................

        using super                 = impl::mapped_file;

    public: // ...............................................................

        mapped_ofile (fs::path const & file, pos_t const reserve_size,
                      clobber::enum_t const cm = clobber::error, advice_dir::enum_t const advice = advice_dir::forward);
        ~mapped_ofile () VR_NOEXCEPT; // calls 'truncate_and_close ()' with inferred size


        // MUTATORs:

        template<bool CHECK_INPUT = VR_CHECK_INPUT>
        addr_t
        advance (pos_t const step, window_t const window);

        template<bool CHECK_INPUT = VR_CHECK_INPUT>
        addr_t
        seek (pos_t const position, window_t const window);

        /**
         * @throws invalid_input if 'size' is greater that the extent of data that could have been written
         */
        bool truncate_and_close (pos_t const size);

}; // end of class
//............................................................................
//............................................................................

template<bool CHECK_INPUT>
VR_FORCEINLINE addr_const_t
mapped_ifile::advance (pos_t const step, window_t const window)
{
    DLOG_trace2 << "advance (" << step << ", " << window << ')';

    return seek_impl<PROT_READ, CHECK_INPUT> (m_offset + step + m_base_position, window);
}

template<bool CHECK_INPUT>
VR_FORCEINLINE addr_const_t
mapped_ifile::seek (pos_t const position, window_t const window)
{
    DLOG_trace2 << "seek (" << position << ", " << window << ')';

    return seek_impl<PROT_READ, CHECK_INPUT> (position, window);
}
//............................................................................
//............................................................................

template<bool CHECK_INPUT>
VR_FORCEINLINE addr_t
mapped_ofile::advance (pos_t const step, window_t const window)
{
    DLOG_trace2 << "[size: " << m_size << "] advance (" << step << ", " << window << ')';

    pos_t const position = m_offset + step + m_base_position;
    addr_t const r = const_cast<addr_t> (seek_impl<(PROT_READ | PROT_WRITE), CHECK_INPUT> (position, window));

    m_size = std::max (m_size, position + window); // update inferred size
    return r;
}

template<bool CHECK_INPUT>
VR_FORCEINLINE addr_t
mapped_ofile::seek (pos_t const position, window_t const window)
{
    DLOG_trace2 << "[size: " << m_size << "] seek (" << position << ", " << window << ')';

    addr_t const r = const_cast<addr_t> (seek_impl<(PROT_READ | PROT_WRITE), CHECK_INPUT> (position, window));

    m_size = std::max (m_size, position + window); // update inferred size
    return r;
}

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
