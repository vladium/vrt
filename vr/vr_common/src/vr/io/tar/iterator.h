#pragma once

#include "vr/asserts.h"
#include "vr/filesystem.h"

#include <boost/iterator/iterator_facade.hpp>

#include <memory>

class archive; // forward

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
//............................................................................
//............................................................................
namespace tar
{
class iterator; // forward

class entry final
{
    public: // ...............................................................

        std::string const & name () const
        {
            return m_name;
        }

        signed_size_t const & size () const
        {
            return m_size;
        }

        /**
         * @param buf [note: caller ensures capacity at least 'size ()']
         */
        VR_ASSUME_HOT signed_size_t read (int8_t * const buf) const;

    private: // ..............................................................

        friend class iterator;

        entry (iterator & parent) :
            m_parent { parent }
        {
        }


        iterator & m_parent;
        std::string m_name;
        signed_size_t m_size { -1 };

}; // end of class
//............................................................................
/*
 * TODO mmap()-ed version would be particularly efficient at skipping
 */
class iterator final: public boost::iterator_facade<iterator, const entry, boost::forward_traversal_tag>
{
    public: // ...............................................................

        /**
         * creates the "end" iterator
         */
        iterator () VR_NOEXCEPT = default;

        /**
         * creates the "begin" iterator
         *
         * @note the underlying API (libarchive) handles gzip-compressed archives, among other formats
         */
        explicit
        iterator (fs::path const & file);

        ~iterator () VR_NOEXCEPT = default;

    private: // ..............................................................

        // the end iterator is indicated by (! m_impl || ! m_impl->m_handle)

        friend class entry;
        friend class boost::iterator_core_access;

        using archive_free_fn_t         = int32_t (*)(::archive *);

        struct iterator_impl final
        {
            iterator_impl (iterator & parent);
            ~iterator_impl () VR_NOEXCEPT; // always calls 'close()'

            // entry I/O support (inline to alter linkage):

            inline void set_entry_buf (int64_t const buf_capacity);
            inline void reset_entry_buf ();

            void close () VR_NOEXCEPT;

            std::unique_ptr<::archive, archive_free_fn_t> m_handle;
            std::unique_ptr<int8_t [/* io_data_buf_size */]> m_iter_buf; // TODO use large obj allocator (align on page boundary, pool, etc)
            signed_size_t m_buf_capacity { io_iter_buf_size }; // reduce I/O costs when iterating, use a larger buffer to actually extract entries
            entry m_current;
            int32_t m_fd { -1 };

        }; // end of nested class


        // iterator_facade:

        VR_ASSUME_HOT void increment ();

        const entry & dereference () const
        {
            assert_condition (m_impl); // attempt to dereference end iterator
            return m_impl->m_current;
        }

        bool equal (const iterator & rhs) const
        {
            return (m_impl == rhs.m_impl)
                || (! m_impl && rhs.m_impl && ! rhs.m_impl->m_handle)
                || (! rhs.m_impl && m_impl && ! m_impl->m_handle);
        }


        static const signed_size_t io_iter_buf_size     = 4 * 1024;
        static const signed_size_t io_data_buf_size     = 128 * 1024;

        vr_static_assert (io_data_buf_size >= io_iter_buf_size);

        // these are 'inline' to alter linkage:

        static inline signed_size_t read_callback (::archive * const a, void/*  iterator_impl */ * const ctx, const void ** const buffer);
        static inline signed_size_t skip_callback (::archive * const a, void/*  iterator_impl */ * const ctx, int64_t const request);


        std::shared_ptr<iterator_impl> const m_impl;

}; // end of class
//............................................................................

//  enable c++11 range-based for-statement use:

inline
const iterator &
begin (const iterator & i) VR_NOEXCEPT
{
    return i;
}

inline
iterator
end (const iterator &) VR_NOEXCEPT
{
    return { };
}

} // end of 'tar'
//............................................................................
//............................................................................

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
