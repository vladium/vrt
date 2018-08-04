
#include "vr/test/mc.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace test
{
//............................................................................

void
task::start ()
{
    m_f_thread = boost::thread { std::ref (* m_f_runnable) };
}

void
task::stop ()
{
    m_f_thread.join ();
}
//............................................................................

std::string const &
task::name () const
{
    assert_nonnull (m_f_runnable);
    return m_f_runnable->name ();
}
//............................................................................
//............................................................................

void
task_container::start ()
{
    for (auto & t : m_tasks)
    {
        t.start ();
    }
}

void
task_container::stop ()
{
    for (auto & t : m_tasks)
    {
        t.stop ();
    }
}
//............................................................................

void
task_container::add (task && t, std::string const & name)
{
    if (name.empty ())
    {
        m_tasks.emplace_back (std::move (t));
    }
    else
    {
        auto i = m_name_map.emplace (name, nullptr);

        if (VR_LIKELY (i.second))
        {
            m_tasks.emplace_back (std::move (t));
            i.first->second = & m_tasks.back ();
        }
        else
            throw_x (invalid_input, "duplicate task name " + print (name));
    }
}
//............................................................................

task const &
task_container::operator[] (int32_t const index) const
{
    check_within (index, m_tasks.size ());

    return m_tasks [index];
}

task const &
task_container::operator[] (std::string const & name) const
{
    auto const i = m_name_map.find (name);
    if (VR_LIKELY (i != m_name_map.end ()))
        return { * i->second };

    throw_x (invalid_input, "no task named " + print (name));
}

} // end of 'test'
} // end of namespace
//----------------------------------------------------------------------------
