
#include "vr/io/hdf/HDF_frame_streams.h"

#include "vr/data/dataframe.h"
#include "vr/io/defs.h"
#include "vr/util/logging.h"
#include "vr/utility.h"

#if VR_USE_BLOSC // TODO ASX-60
#   include <blosc_filter.h>
#endif

#include <hdf5.h>

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
using namespace data;

//............................................................................

#define vr_ATYPE_ATTR_NAME      "vr_at"
#define vr_LABELS_ATTR_NAME     "vr_labels"

//............................................................................

#define vr_CHECKED_H5_CALL(exp) \
    ({ \
       decltype (exp) rc; rc = (exp); \
       if (VR_UNLIKELY (rc < 0)) \
           { throw_x (io_exception, "(" #exp ") failed"); }; /* TODO proper error handling */ \
       rc; \
    }) \
    /* */

#define vr_CHECKED_H5_CALL_noexcept(exp) \
    ({ \
       decltype (exp) rc; rc = (exp); \
       if (VR_UNLIKELY (rc < 0)) \
           { LOG_error << "(" #exp ") failed"; }; /* TODO proper error handling */ \
       rc; \
    }) \
    /* */

#define vr_CHECKED_H5_CALL_noexcept_file(exp, file) \
    ({ \
       decltype (exp) rc; rc = (exp); \
       if (VR_UNLIKELY (rc < 0)) \
           { LOG_error << "(" #exp ") failed " << print (file); }; /* TODO proper error handling */ \
       rc; \
    }) \
    /* */

//............................................................................
//............................................................................
namespace
{
//............................................................................

vr_static_assert (sizeof (::hid_t) == sizeof (bitset32_t));

int32_t const chunk_size                    = 4 * 1024; // TODO experiment TODO make this a parm

int32_t const zlib_default_compression      = 6;

#if VR_USE_BLOSC
int32_t const blosc_default_compression     = 5;
int32_t const blosc_default_shuffle         = 1;
#endif
//............................................................................

uint32_t
mode_to_HDF_flags (bitset32_t const mode)
{
    uint32_t r { };

    if (O_TRUNC & mode) r |= H5F_ACC_TRUNC;
    if (O_EXCL & mode)  r |= H5F_ACC_EXCL;

    return r;
}
//............................................................................
// TODO
// - HDF no longer supports a datetime type
// - is it worthwhile using HDF enum support (e.g. for compression)?

template<atype::enum_t ATYPE, typename = void>
struct attr_HDF_traits
{
    using data_type                 = typename atype::traits<ATYPE>::type;

    static constexpr ::hid_t storage_type () { return (sizeof (data_type) == 4 ? H5T_NATIVE_INT32 : H5T_NATIVE_INT64); }

}; // end of master

template<atype::enum_t ATYPE>
struct attr_HDF_traits<ATYPE, // specialized for fp attributes
    typename std::enable_if<std::is_floating_point<typename atype::traits<ATYPE>::type>::value>::type>
{
    using data_type                 = typename atype::traits<ATYPE>::type;

    static constexpr ::hid_t storage_type () { return (sizeof (data_type) == 4 ? H5T_NATIVE_FLOAT : H5T_NATIVE_DOUBLE); }

}; // end of specialization
//............................................................................

struct HDF_frame_stream_state_base
{
    HDF_frame_stream_state_base (fs::path const & file, bitset32_t const mode, arg_map const & parms) :
        m_file { file },
        m_mode { mode },
        m_filter { parms.get<filter> ("filter", filter::none) }
    {
        if (m_filter != filter::none)
        {
            for (int32_t a = 0; true; ++ a)
            {
                std::string const parm_name = "filter." + string_cast (a);

                auto const i = parms.find (parm_name);
                if (i == parms.end ())
                    break;
                else
                    m_filter_parms.push_back (i->second);
            }
        }

        LOG_trace1 << "configured with " << print (m_filter) << " filter, " << m_filter_parms.size () << " filter parm(s)";
    }


    ~HDF_frame_stream_state_base () VR_NOEXCEPT
    {
        if (! m_attr_dset_IDs.empty ())
        {
            for (auto const dset : m_attr_dset_IDs)
            {
                if (dset >= 0) vr_CHECKED_H5_CALL_noexcept_file (::H5Dclose (dset), m_file);
            }

            m_attr_dset_IDs.clear ();
        }

        close_mem_space ();

        ::hid_t const file = m_file_ID;
        if (file >= 0)
        {
            m_file_ID = -1;
            vr_CHECKED_H5_CALL_noexcept_file (::H5Fclose (file), m_file);
        }
    }


    VR_ASSUME_COLD void create ()
    {
        try
        {
            // create a property list so we can enable link order tracking (used for 'attr_schema' attr ordering):

            ::hid_t const fcpl = vr_CHECKED_H5_CALL (::H5Pcreate (H5P_FILE_CREATE));
            VR_SCOPE_EXIT ([fcpl]() { ::H5Pclose (fcpl); });

            vr_CHECKED_H5_CALL (::H5Pset_link_creation_order (fcpl, H5P_CRT_ORDER_TRACKED));


            uint32_t const flags = mode_to_HDF_flags (m_mode) /*| (H5F_ACC_RDWR)*/;
            LOG_trace2 << "using create flags 0x" << std::hex << flags;

            m_file_ID = vr_CHECKED_H5_CALL (::H5Fcreate (m_file.c_str (), flags, fcpl, H5P_DEFAULT));
            LOG_trace1 << "created [" << m_file.string () << "] file ID " << m_file_ID;

            open_mem_space ();
        }
        catch (...)
        {
            chain_x (io_exception, "failed to create " + print (m_file));
        }
    }

    VR_ASSUME_COLD void open ()
    {
        uint32_t const flags = mode_to_HDF_flags (m_mode) | (H5F_ACC_RDONLY);
        LOG_trace2 << "using open flags 0x" << std::hex << flags;

        try
        {
            m_file_ID = vr_CHECKED_H5_CALL (::H5Fopen (m_file.c_str (), flags, H5P_DEFAULT));
            LOG_trace1 << "opened [" << m_file.string () << "] as file ID " << m_file_ID;

            open_mem_space ();
        }
        catch (...)
        {
            chain_x (io_exception, "failed to open " + print (m_file));
        }
    }


    VR_ASSUME_COLD void open_mem_space ()
    {
        m_mem_space_size            = 0;
        ::hsize_t const size_max    = H5S_UNLIMITED;
        m_mem_space_ID = vr_CHECKED_H5_CALL (::H5Screate_simple (1, & m_mem_space_size, & size_max));
        LOG_trace1 << "opened mem space ID " << m_mem_space_ID;
    }

    VR_ASSUME_COLD void close_mem_space () VR_NOEXCEPT
    {
        ::hid_t const mem_space = m_mem_space_ID;
        if (mem_space >= 0)
        {
            m_mem_space_ID = -1;
            vr_CHECKED_H5_CALL_noexcept_file (::H5Sclose (mem_space), m_file);
        }
    }

    VR_FORCEINLINE ::hid_t resize_mem_space (::hsize_t const size) // note: keep force-inlined if the size check is done by this method
    {
        ::hid_t const r = m_mem_space_ID;
        assert_nonnegative (r); // has been created

        if (VR_UNLIKELY (m_mem_space_size != size))
        {
            m_mem_space_size        = size;
            vr_CHECKED_H5_CALL (:: H5Sset_extent_simple (r, 1, & m_mem_space_size, nullptr));
        }

        return r;
    }


    fs::path const m_file;
    std::vector<::hid_t> m_attr_dset_IDs;
    ::hid_t m_file_ID { -1 };
    ::hid_t m_mem_space_ID { -1 }; // re-used for all attribute datasets
    dataframe::size_type m_rows_done { };
    ::hsize_t m_mem_space_size { }; // curent size of 'm_mem_space_ID'
    bitset32_t const m_mode;
    filter::enum_t const m_filter;
    std::vector<any> m_filter_parms;

}; // end of class
//............................................................................

struct dataset_reader final // stateless
{
    template<atype::enum_t ATYPE>
    VR_FORCEINLINE void
    operator() (atype_<ATYPE>,
                addr_t const col_raw, dataframe::size_type const rows_read, dataframe::size_type const row_count, ::hid_t const mem_space_ID, ::hid_t const dset_ID, ::hid_t const dset_space_ID) const
    {
        assert_positive (row_count); // shouldn't be invoked if there's no I/O to do

        auto const storage_type     = attr_HDF_traits<ATYPE>::storage_type ();

        // set target dataset hyperslab:
        {
            ::hsize_t const start       = rows_read;
            ::hsize_t const count       = row_count;

            vr_CHECKED_H5_CALL (::H5Sselect_hyperslab (dset_space_ID, ::H5S_SELECT_SET, & start, nullptr, & count, nullptr));
        }

        // read actual data:

        vr_CHECKED_H5_CALL (::H5Dread (dset_ID, storage_type, mem_space_ID, dset_space_ID, H5P_DEFAULT, col_raw));
    }

}; // end of class
//............................................................................

struct dataset_creator final // stateless
{
    template<atype::enum_t ATYPE>
    VR_FORCEINLINE void
    operator() (atype_<ATYPE>,
                attribute const & a, HDF_frame_stream_state_base & state) const
    {
        auto const storage_type     = attr_HDF_traits<ATYPE>::storage_type ();

        // TODO H5Pset_cache() for global settings and H5Pset_chunk_cache() for per-dataset settings

        // create a chunked 1D dataset, with unlimited max size and initial size of zero (will be extended as needed):

        ::hsize_t const dset_data_size      = 0;
        ::hsize_t const dset_data_size_max  = H5S_UNLIMITED;
        ::hid_t const dset_space = vr_CHECKED_H5_CALL (::H5Screate_simple (1, & dset_data_size, & dset_data_size_max));
        VR_SCOPE_EXIT ([dset_space]() { ::H5Sclose (dset_space); }); // won't be able to re-use the same space as the dataset gets appended to

        ::hid_t const dset_cpl = vr_CHECKED_H5_CALL (::H5Pcreate (H5P_DATASET_CREATE));
        VR_SCOPE_EXIT ([dset_cpl]() { ::H5Pclose (dset_cpl); }); // won't need again after 'a_ctx.m_dset_ID' is set

        switch (state.m_filter)
        {
            case filter::none: break;

            case filter::zlib:
            {
                int32_t const level   = (state.m_filter_parms.size () > 0 ? any_get<int32_t> (state.m_filter_parms [0]) : zlib_default_compression);

                LOG_trace1 << "  setting attribute (" << print (a) << ") zlib compression level to " << level;
                vr_CHECKED_H5_CALL (::H5Pset_deflate (dset_cpl, level));
            }
            break;

#        if VR_USE_BLOSC
            case filter::blosc:
            {
                int32_t const level   = (state.m_filter_parms.size () > 0 ? any_get<int32_t> (state.m_filter_parms [0]) : blosc_default_compression);
                int32_t const shuffle = (state.m_filter_parms.size () > 1 ? any_get<int32_t> (state.m_filter_parms [1]) : blosc_default_shuffle);

                uint32_t cd_values [7] { 0 };

                // [note: 0 to 3 (inclusive) param slots are reserved

                cd_values [4] = level;          // compression level
                cd_values [5] = shuffle;        // 0: shuffle not active, 1: shuffle active
                cd_values [6] = BLOSC_BLOSCLZ;  // actual compressor to use

                LOG_trace1 << "  setting blosc v" << FILTER_BLOSC_VERSION << " compression level to " << level << ", shuffle to " << shuffle;
                vr_CHECKED_H5_CALL (::H5Pset_filter (dset_cpl, FILTER_BLOSC, H5Z_FLAG_OPTIONAL, length (cd_values), cd_values));
            }
            break;
#        endif // VR_USE_BLOSC

            default: VR_ASSUME_UNREACHABLE ();

        } // end of switch

        ::hsize_t const dset_chunk_dim      = chunk_size;
        vr_CHECKED_H5_CALL (::H5Pset_chunk (dset_cpl, 1, & dset_chunk_dim));
        check_eq (::H5Pget_layout (dset_cpl), ::H5D_CHUNKED);

        ::hid_t const dset_ID = vr_CHECKED_H5_CALL (::H5Dcreate2 (state.m_file_ID, a.name ().c_str (), storage_type, dset_space, H5P_DEFAULT, dset_cpl, H5P_DEFAULT));
        state.m_attr_dset_IDs.push_back (dset_ID); // destructor releases 'dset_ID'

        LOG_trace1 << "  created attribute (" << print (a) << ") dataset " << dset_ID;

        // use an HDF attribute to store 'a.type ()':

        ::hsize_t const a_type_size         = 1;
        ::hid_t const a_type_space = vr_CHECKED_H5_CALL (::H5Screate_simple (1, & a_type_size, nullptr));
        VR_SCOPE_EXIT ([a_type_space]() { ::H5Sclose (a_type_space); });

        switch (a.type ().atype ())
        {
            case atype::category: // store category labels in a separate HDF attribute
            {
                // TODO: can use soft links to share common 'category' label arrays/atypes

                ::hid_t const vls_type_ID = vr_CHECKED_H5_CALL (::H5Tcopy (H5T_C_S1));
                VR_SCOPE_EXIT ([vls_type_ID]() { ::H5Tclose (vls_type_ID); });

                vr_CHECKED_H5_CALL (::H5Tset_size (vls_type_ID, H5T_VARIABLE));

                ::hsize_t const label_array_size = a.type ().labels ().size ();
                ::hid_t const label_array_type = vr_CHECKED_H5_CALL (::H5Tarray_create2 (vls_type_ID, 1, & label_array_size));
                VR_SCOPE_EXIT ([label_array_type]() { ::H5Tclose (label_array_type); });

                ::hid_t const a_type_attr = vr_CHECKED_H5_CALL (::H5Acreate2 (dset_ID, vr_LABELS_ATTR_NAME, label_array_type, a_type_space, H5P_DEFAULT, H5P_DEFAULT));
                VR_SCOPE_EXIT ([a_type_attr]() { ::H5Aclose (a_type_attr); });

                std::unique_ptr<string_literal_t []> const data { boost::make_unique_noinit<string_literal_t []> (label_array_size) };
                for (int32_t l = 0, l_limit = label_array_size; l < l_limit; ++ l)
                {
                    data [l] = a.type ().labels ()[l].c_str ();
                }

                vr_CHECKED_H5_CALL (::H5Awrite (a_type_attr, label_array_type, data.get ()));
            }
            // !!! FALL THROUGH !!!
            /* no break */

            default: // store the 'atype::enum_t' value
            {
                ::hid_t const a_type_attr = vr_CHECKED_H5_CALL (::H5Acreate2 (dset_ID, vr_ATYPE_ATTR_NAME, H5T_NATIVE_UINT8, a_type_space, H5P_DEFAULT, H5P_DEFAULT));
                VR_SCOPE_EXIT ([a_type_attr]() { ::H5Aclose (a_type_attr); });

                uint8_t const a_atype = a.type ().atype ();
                vr_CHECKED_H5_CALL (::H5Awrite (a_type_attr, H5T_NATIVE_UINT8, & a_atype));
            }
            break;

        } // end of switch
    }

}; // end of class
//............................................................................

struct dataset_writer final // stateless
{
    template<atype::enum_t ATYPE>
    VR_FORCEINLINE void
    operator() (atype_<ATYPE>,
                addr_const_t const col_raw, dataframe::size_type const rows_written, dataframe::size_type const row_count, ::hid_t const mem_space_ID, ::hid_t const dset_ID) const
    {
        assert_positive (row_count); // shouldn't be invoked if there's no I/O to do

        auto const storage_type     = attr_HDF_traits<ATYPE>::storage_type ();

        // extend the dataset:
        {
            ::hsize_t const dset_extent = rows_written + row_count;

            vr_CHECKED_H5_CALL (::H5Dset_extent (dset_ID, & dset_extent));
        }

        // get (updated) dataset file space:

        ::hid_t const dset_space = vr_CHECKED_H5_CALL (::H5Dget_space (dset_ID));
        VR_SCOPE_EXIT ([dset_space]() { ::H5Sclose (dset_space); }); // 'H5Dget_space()' makes a copy that needs to be released

        // set target dataset hyperslab:
        {
            ::hsize_t const start       = rows_written;
            ::hsize_t const count       = row_count;

            vr_CHECKED_H5_CALL (::H5Sselect_hyperslab (dset_space, ::H5S_SELECT_SET, & start, nullptr, & count, nullptr));
        }

        // write actual data:

        vr_CHECKED_H5_CALL (::H5Dwrite (dset_ID, storage_type, mem_space_ID, dset_space, H5P_DEFAULT, col_raw));
    }

}; // end of class

} // end of anonymous
//............................................................................
//............................................................................
namespace impl
{

struct HDF_frame_istream_base::state final: public HDF_frame_stream_state_base
{
    state (fs::path const & file, bitset32_t const mode, arg_map const & parms) :
        HDF_frame_stream_state_base (file, mode, parms)
    {
        open (); // an istream opens eagerly
    }

    ~state () VR_NOEXCEPT
    {
        if (! m_attr_dset_space_IDs.empty ())
        {
            for (auto const dset_space : m_attr_dset_space_IDs)
            {
                if (dset_space >= 0) vr_CHECKED_H5_CALL_noexcept_file (::H5Sclose (dset_space), m_file);
            }

            m_attr_dset_space_IDs.clear ();
        }
    }

    dataframe::size_type m_rows_total { };
    std::vector<::hid_t> m_attr_dset_space_IDs;

}; // end of nested class
//............................................................................

struct visit_dataset_context final
{
    void/* HDF_frame_istream_base::state */* m_state;
    std::vector<attribute> m_attrs;

}; // end of class
//............................................................................

template<>
struct HDF_frame_istream_base::access_by<void>
{
    static ::herr_t visit_dataset (::hid_t const g_ID, string_literal_t const name, ::H5L_info_t const * info, void * ctx)
    {
        LOG_trace2 << "  \"" << name << "\": " << info->type;

        assert_condition (ctx);
        visit_dataset_context & vctx = * static_cast<visit_dataset_context *> (ctx);
        HDF_frame_istream_base::state & this_= * static_cast<HDF_frame_istream_base::state *> (vctx.m_state);

        check_condition (info->corder_valid);
        check_eq (info->type, ::H5L_TYPE_HARD);

        ::H5O_info_t obj_info;
        vr_CHECKED_H5_CALL (::H5Oget_info_by_name (g_ID, name, & obj_info, H5P_DEFAULT));

        LOG_trace2 << "  \"" << name << "\": type " << obj_info.type << ", " << obj_info.num_attrs << " HDF attr(s)";

        check_eq (obj_info.type, ::H5O_TYPE_DATASET);
        check_le (obj_info.num_attrs, 2);

        // cache this column's dataset ID:

        assert_nonnegative (this_.m_file_ID); // the file has been opened

        ::hid_t const dset_ID = vr_CHECKED_H5_CALL (::H5Dopen2 (this_.m_file_ID, name, H5P_DEFAULT));
        this_.m_attr_dset_IDs.push_back (dset_ID); // destructor releases 'dset_ID'

        // read 'vr_ATYPE_ATTR_NAME' HDF attribute:

        LOG_trace1 << "  opened attribute (\"" << name << "\") dataset " << dset_ID;

        ::hid_t const a_type_attr = vr_CHECKED_H5_CALL (::H5Aopen (dset_ID, vr_ATYPE_ATTR_NAME, H5P_DEFAULT));
        VR_SCOPE_EXIT ([a_type_attr]() { ::H5Aclose (a_type_attr); });

        atype::enum_t at;
        {
            uint8_t a_atype { };
            vr_CHECKED_H5_CALL (::H5Aread (a_type_attr, H5T_NATIVE_UINT8, & a_atype));

            at = static_cast<atype::enum_t> (a_atype);
        }

        switch (at)
        {
            case atype::category:
            {
                string_vector labels;
                {
                    // read 'vr_LABELS_ATTR_NAME' attribute:

                    ::hid_t const labels_attr = vr_CHECKED_H5_CALL (::H5Aopen (dset_ID, vr_LABELS_ATTR_NAME, H5P_DEFAULT));
                    VR_SCOPE_EXIT ([labels_attr]() { ::H5Aclose (labels_attr); });

                    ::hid_t const labels_attr_type = vr_CHECKED_H5_CALL (::H5Aget_type (labels_attr));
                    VR_SCOPE_EXIT ([labels_attr_type]() { ::H5Tclose (labels_attr_type); });

                    ::hsize_t label_array_size { };

                    assert_eq (::H5Tget_array_ndims (labels_attr_type), 1);
                    vr_CHECKED_H5_CALL (::H5Tget_array_dims2 (labels_attr_type, & label_array_size));

                    LOG_trace2 << "    attribute (\"" << name << "\") is categoric with " << label_array_size << " label(s)";

                    {
                        std::unique_ptr<string_literal_t []> const data { boost::make_unique_noinit<string_literal_t []> (label_array_size) };
                        vr_CHECKED_H5_CALL (::H5Aread (labels_attr, labels_attr_type, data.get ()));

                        for (int32_t l = 0, l_limit = label_array_size; l < l_limit; ++ l)
                        {
                            string_literal_t const s = data [l];

                            labels.push_back (s); // note: this copies 's' into an 'std::string'
                            std::free (const_cast<char *> (s));
                        }
                    }
                }
                vctx.m_attrs.emplace_back (name, label_array::create_and_intern (std::move (labels))); // last use of 'labels'
            }
            break;

            default:
            {
                LOG_trace2 << "    attribute (\"" << name << "\") is numeric";

                vctx.m_attrs.emplace_back (name, at);
            }
            break;

        } // end of switch

        // cache the (immutable) file space ID for this dataset:

        ::hid_t const dset_space_ID = vr_CHECKED_H5_CALL (::H5Dget_space (dset_ID));
        this_.m_attr_dset_space_IDs.push_back (dset_space_ID); // destructor releases 'dset_space_ID'

        ::hssize_t const rows = vr_CHECKED_H5_CALL (::H5Sget_simple_extent_npoints (dset_space_ID));
        if (VR_LIKELY (this_.m_rows_total))
        {
            check_eq (rows, this_.m_rows_total); // check dataframe consistency
        }
        else // set the total row count on the first visit:
        {
            this_.m_rows_total = rows;

            LOG_trace1 << "  row count = " << rows;
        }

        return 0; // continue to iterate
    }

}; // end of class
//............................................................................
// HDF_frame_istream_base:
//............................................................................

HDF_frame_istream_base::HDF_frame_istream_base (fs::path const & file, bitset32_t const mode, arg_map const & parms) :
    m_state { std::make_unique<state> (file, mode, parms) }
{
    initialize (); // an ostream opens lazily
}

HDF_frame_istream_base::HDF_frame_istream_base (HDF_frame_istream_base && rhs) = default; // pimpl

HDF_frame_istream_base::~HDF_frame_istream_base ()
{
    close ();
}
//............................................................................

void
HDF_frame_istream_base::initialize ()
{
    assert_condition (m_state);
    assert_condition (! m_schema);

    state & this_ = * m_state;
    assert_nonnegative (this_.m_file_ID); // the file has been opened

    {
        visit_dataset_context vctx { m_state.get () };

        vr_CHECKED_H5_CALL (::H5Literate (this_.m_file_ID, ::H5_INDEX_CRT_ORDER, ::H5_ITER_INC, nullptr, HDF_frame_istream_base::access_by<void>::visit_dataset, & vctx));

        m_schema.reset (new attr_schema { std::move (vctx.m_attrs) } ); // last use of 'vctx'
    }

    LOG_trace1 << "HDF istream initialized with schema\n" << print (* m_schema);
}

void
HDF_frame_istream_base::close ()
{
    if (m_state)
    {
        LOG_trace1 << "HDF istream read " << m_state->m_rows_done << " row(s)";

        m_state.reset ();
    }
}
//............................................................................

int64_t
HDF_frame_istream_base::read (data::dataframe & dst)
{
    if (VR_UNLIKELY (! m_state)) // guard against using a close()d stream
        return -1;

    assert_condition (m_schema); // set by initialize() called from the constructor

    if (VR_UNLIKELY (m_last_io_df != & dst))
    {
        // an imprecise, but fast, schema consistency check:

        check_eq (dst.schema ()->size (), m_schema->size ());
        assert_eq (hash_value (* dst.schema ()), hash_value (* m_schema));
    }


    dataframe::size_type const dst_row_capacity = dst.row_capacity ();
    assert_positive (dst_row_capacity);

    state & this_ = * m_state;

    dataframe::size_type const rows_read = this_.m_rows_done;
    dataframe::size_type const r = std::min<dataframe::size_type> (this_.m_rows_total - rows_read, dst_row_capacity);
    assert_positive (r);

    auto const mem_space = this_.resize_mem_space (r); // once for all column datasets

    attr_schema const & as = * m_schema;
    width_type const col_count { as.size () };

    for (width_type c = 0; c < col_count; ++ c)
    {
        attribute const & a = as [c];

        dispatch_on_atype (a.atype (), dataset_reader { }, dst.at_raw<false> (c), rows_read, r, mem_space, this_.m_attr_dset_IDs [c], this_.m_attr_dset_space_IDs [c]); // TODO check inlining
    }

    dst.resize_row_count (r);

    this_.m_rows_done = rows_read + r;


    if (VR_UNLIKELY (this_.m_rows_done >= this_.m_rows_total))
    {
        LOG_trace1 << "EOF, closing the stream";
        close ();
    }

    m_last_io_df = & dst; // TODO can also cache col raw addrs
    return r;
}
//............................................................................
//............................................................................

struct HDF_frame_ostream_base::state final: public HDF_frame_stream_state_base
{
    state (fs::path const & file, bitset32_t const mode, arg_map const & parms) :
        HDF_frame_stream_state_base (file, mode, parms)
    {
    }

}; // end of nested class
//............................................................................
// HDF_frame_ostream_base:
//............................................................................

HDF_frame_ostream_base::HDF_frame_ostream_base (fs::path const & file, bitset32_t const mode, arg_map const & parms) :
    m_state { std::make_unique<state> (file, mode, parms) }
{
}

HDF_frame_ostream_base::HDF_frame_ostream_base (HDF_frame_ostream_base && rhs) = default; // pimpl

HDF_frame_ostream_base::~HDF_frame_ostream_base ()
{
    close ();
}
//............................................................................

void
HDF_frame_ostream_base::initialize (data::attr_schema::ptr const & schema)
{
    assert_condition (m_state);
    state & this_ = * m_state;

    switch (this_.m_filter)
    {
        case filter::none: break;

        case filter::zlib:
        {
            // check that zlib is available for both compression and decompression:

            check_positive (::H5Zfilter_avail (H5Z_FILTER_DEFLATE));

            uint32_t filter_info { };
            vr_CHECKED_H5_CALL (:: H5Zget_filter_info (H5Z_FILTER_DEFLATE, & filter_info));

            check_condition ((filter_info & H5Z_FILTER_CONFIG_ENCODE_ENABLED) && (filter_info & H5Z_FILTER_CONFIG_DECODE_ENABLED));
        }
        break;

#    if VR_USE_BLOSC
        case filter::blosc:
        {
            // TODO check that the filter can be loaded?
        }
        break;
#    endif //VR_USE_BLOSC

        default: throw_x (invalid_input, "unsupported filter type " + print (this_.m_filter));

    } // end of switch

    attr_schema const & as = * (m_schema = schema);
    LOG_trace1 << "HDF ostream initialized with schema\n" << print (as);

    this_.create (); // unlike an istream, an ostream creates/opens the file lazily, on first actual 'write()'

    // create attribute datasets (note that creation order is tracked and is thus important for correctness):
    {
        for (width_type c = 0, c_limit = as.size (); c < c_limit; ++ c)
        {
            attribute const & a = as [c];

            dispatch_on_atype (a.atype (), dataset_creator { }, a, this_);
        }
    }
}

void
HDF_frame_ostream_base::close ()
{
    if (m_state)
    {
        LOG_trace1 << "HDF ostream wrote " << m_state->m_rows_done << " row(s)";

        m_state.reset ();
    }
}
//............................................................................

int64_t
HDF_frame_ostream_base::write (data::dataframe const & src)
{
    check_condition (m_state); // guard against using a close()d stream

    if (VR_UNLIKELY (! m_schema))
    {
        initialize (src.schema ());
        assert_condition (!! m_schema);            // TODO make this more exception-safe:
    }
    else if (VR_UNLIKELY (m_last_io_df != & src))
    {
        // an imprecise, but fast, schema consistency check:

        check_eq (src.schema ()->size (), m_schema->size ());
        assert_eq (hash_value (* src.schema ()), hash_value (* m_schema));
    }

    dataframe::size_type const src_row_count = src.row_count ();
    assert_positive (src_row_count);

    attr_schema const & as = * m_schema;
    state & this_ = * m_state;

    auto const mem_space = this_.resize_mem_space (src_row_count); // once for all column datasets

    width_type const col_count { as.size () };
    dataframe::size_type const rows_written = this_.m_rows_done;

    for (width_type c = 0; c < col_count; ++ c)
    {
        attribute const & a = as [c];

        dispatch_on_atype (a.atype (), dataset_writer { }, src.at_raw<false> (c), rows_written, src_row_count, mem_space, this_.m_attr_dset_IDs [c]); // TODO check inlining
    }

    this_.m_rows_done = rows_written + src_row_count;

    m_last_io_df = & src; // TODO can also cache col raw addrs
    return src_row_count;
}

} // end of 'impl'
//............................................................................
//............................................................................

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
