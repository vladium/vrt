#pragma once

#include "vr/fields.h"
#include "vr/io/defs.h" // ts_local/origin
#include "vr/util/datetime.h"
#include "vr/util/ops_int.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
//............................................................................

#define VR_PCAP_MAGIC_LE_NS     0xA1B23C4D

//............................................................................
/**
 * @ref https://wiki.wireshark.org/Development/LibpcapFileFormat
 */
template<typename DERIVED, bool LE = true>
class pcaprec_hdr_wire
{
    private: // ..............................................................

        using uint32_LE         = util::net_int<uint32_t, LE>;

        struct timestamp_access
        {
            static constexpr auto get ()    { return [](DERIVED const & this_) { return this_.get_timestamp (); }; }
            static constexpr auto set ()    { return [](DERIVED & this_, timestamp_t const ts) { this_.set_timestamp (ts); }; }

        }; // end of nested scope

    public: // ...............................................................

        static constexpr bool little_endian ()      { return LE; };

        // ACCESSORs:

        uint32_t ts_sec () const
        {
            return m_ts_sec;
        }

        uint32_t ts_fraction () const
        {
            return m_ts_fraction;
        }

        uint32_t incl_len () const
        {
            return m_incl_len;
        }

        uint32_t orig_len () const
        {
            return m_orig_len;
        }

        // MUTATORs:

        uint32_LE & ts_sec ()
        {
            return m_ts_sec;
        }

        uint32_LE & ts_fraction ()
        {
            return m_ts_fraction;
        }

        uint32_LE & incl_len ()
        {
            return m_incl_len;
        }

        uint32_LE & orig_len ()
        {
            return m_orig_len;
        }

        // expose 'm_ts_sec' and 'm_ts_fraction' as a synthetic meta-field:

        using _meta     = meta::synthetic_meta_t
                        <
                            meta::fdef_<timestamp_access,   _ts_local_>
                        >;

    private: // ..............................................................

        uint32_LE m_ts_sec;          /* timestamp seconds */
        uint32_LE m_ts_fraction;     /* timestamp [micro,nano]seconds, unit determined by 'TS_FRACTION_TYPE' */
        uint32_LE m_incl_len;        /* number of octets of packet saved in file */
        uint32_LE m_orig_len;        /* actual length of packet */

}; // end of class
//............................................................................

template<typename /* util::subsecond_duration_[u,n]s */TS_FRACTION_TYPE, bool LE = true>
class pcaprec_hdr; // master


template<bool LE> // specialize for microsec timestamps
class pcaprec_hdr<util::subsecond_duration_us, LE>: public pcaprec_hdr_wire<pcaprec_hdr<util::subsecond_duration_us, LE>, LE>
{
    private: // ..............................................................

        using super         = pcaprec_hdr_wire<pcaprec_hdr<util::subsecond_duration_us, LE>, LE>;

        static constexpr timestamp_t ts_fraction_scale ()   { return 1000; }

    public: // ...............................................................

        static constexpr timestamp_t ts_fraction_limit ()   { return (_1_second () / ts_fraction_scale ()); }
        static constexpr timestamp_t ts_fraction_digits ()  { return 6; }

        // ACCESSORs:

        VR_FORCEINLINE timestamp_t get_timestamp () const
        {
            return (super::ts_sec () * _1_second () + super::ts_fraction () * ts_fraction_scale ());
        }

        // MUTATORs:

        VR_FORCEINLINE void set_timestamp (timestamp_t const ts)
        {
            super::ts_sec () = (ts / _1_second ());
            super::ts_fraction () = (ts % _1_second ()) / ts_fraction_scale ();
        }

}; // end of specialization

template<bool LE> // specialize for nanosec timestamps
class pcaprec_hdr<util::subsecond_duration_ns, LE>: public pcaprec_hdr_wire<pcaprec_hdr<util::subsecond_duration_ns, LE>, LE>
{
    private: // ..............................................................

        using super         = pcaprec_hdr_wire<pcaprec_hdr<util::subsecond_duration_ns, LE>, LE>;

    public: // ...............................................................

        static constexpr timestamp_t ts_fraction_limit ()   { return _1_second (); }
        static constexpr timestamp_t ts_fraction_digits ()  { return 9; }

        // ACCESSORs:

        VR_FORCEINLINE timestamp_t get_timestamp () const
        {
            return (super::ts_sec () * _1_second () + super::ts_fraction ());
        }

        // MUTATORs:

        VR_FORCEINLINE void set_timestamp (timestamp_t const ts)
        {
            super::ts_sec () = (ts / _1_second ());
            super::ts_fraction () = (ts % _1_second ());
        }

}; // end of specialization

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
