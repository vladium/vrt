#pragma once

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
/**
 * @note by default the destructor rolls back: this design is to stay consistent
 *       with using exceptions for failure; an explicit @ref commit() is required
 *       in the success code path
 */
template<typename CONNECTION>
class tx_guard final: noncopyable
{
    public: // ...............................................................

        tx_guard (CONNECTION & c) :
            m_c { & c }
        {
            m_c->tx_begin ();
        }

        tx_guard (tx_guard && rhs) :
            m_c { rhs.m_c }
        {
            rhs.m_c = nullptr; // take ownership of the TX
        }

        ~tx_guard ()
        {
            rollback ();
        }

        void commit () // idempotent
        {
            CONNECTION * const c = m_c;
            if (c)
            {
                m_c = nullptr;
                c->tx_commit ();
            }
        }

        void rollback () VR_NOEXCEPT // idempotent
        {
            CONNECTION * const c = m_c;
            if (c)
            {
                m_c = nullptr;
                try
                {
                    c->tx_rollback ();
                }
                catch (std::exception const & e)
                {
                    LOG_error << "TX rollback failure: " << e.what ();
                }
                catch (...)
                {
                    LOG_error << "TX rollback failure: unknown reason";
                }
            }
        }

    private: // ..............................................................

        CONNECTION * m_c;

}; // end of class

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
