
#include "vr/io/files.h"
#include "vr/io/sql/sql_connection_factory.h"
#include "vr/io/uri.h"
#include "vr/settings.h"
#include "vr/util/di/container.h"

#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
//............................................................................

TEST (sqlite_connection_factory, create_or_open_db)
{
    fs::path const out_dir { test::unique_test_path () };
    io::create_dirs (out_dir);

    // create a new db:

    for (std::string const mode : { "rwc", "memory" })
    {
        for (std::string const cache : { "private", "shared" })
        {
            fs::path const db_file { out_dir / join_as_name<'.'> ("db", mode, cache) };

            settings const cfg
            {
                { "mode", mode },
                { "cache", cache },
                { "db", db_file.native () },
                { "on_create",
                    {
                        "PRAGMA page_size=4096;",
                        "PRAGMA auto_vacuum=none;"
                    }
                },
                { "on_open",
                    {
                        "PRAGMA read_uncommitted=true;"
                    }
                }
            };

            {
                util::di::container app { join_as_name ("APP", test::current_test_name ()) };

                app.configure ()
                    ("sql", new sql_connection_factory { { { "test_conn", cfg } } })
                ;

                app.start ();
                {
                    sql_connection_factory & sqf = app ["sql"];

                    sql_connection_factory::connection & c = sqf.acquire ("test_conn");
                    sqf.release (c);
                }
                app.stop ();
            }

            if (mode != "memory") { ASSERT_TRUE (fs::exists (db_file)); }
        }
    }

    // open dbs created in the above step:

    for (std::string const mode : { "ro", "rw", "rwc" })
    {
        for (std::string const cache : { "private", "shared" })
        {
            fs::path const db_file { out_dir / join_as_name<'.'> ("db", "rwc", cache) };

            settings const cfg
            {
                { "mode", mode },
                { "cache", cache },
                { "db", uri { db_file }.native () }, // test specifying 'db' as a url
                { "pool_size", 2 },
                { "on_open",
                    {
                        "PRAGMA read_uncommitted=true;"
                    }
                }
            };

            {
                util::di::container app { join_as_name ("APP", test::current_test_name ()) };

                app.configure ()
                    ("sql", new sql_connection_factory { { { "test_conn", cfg } } })
                ;

                app.start ();
                {
                    sql_connection_factory & sqf = app ["sql"];

                    sql_connection_factory::connection & c = sqf.acquire ("test_conn");
                    sqf.release (c);
                }
                app.stop ();
            }
        }
    }
}
//............................................................................

TEST (sqlite_connection_factory, sloppy_release)
{
    fs::path const out_dir { test::unique_test_path () };
    io::create_dirs (out_dir);

    // create a new db:

    for (std::string const mode : { "rwc", "memory" })
    {
        for (std::string const cache : { "private", "shared" })
        {
            fs::path const db_file { out_dir / join_as_name<'.'> ("db", mode, cache) };

            settings cfg
            {
                { "mode", mode },
                { "cache", cache },
                { "db", db_file.native () },
                { "on_create",
                    {
                        "PRAGMA page_size=4096;",
                        "PRAGMA auto_vacuum=none;"
                    }
                },
                { "on_open",
                    {
                        "PRAGMA read_uncommitted=true;"
                    }
                }
            };

            {
                util::di::container app { join_as_name ("APP", test::current_test_name ()) };

                app.configure ()
                    ("sql", new sql_connection_factory { { { "test_conn", cfg } } })
                ;

                app.start ();
                {
                    sql_connection_factory & sqf = app ["sql"];

                    // release only half of acquired connections, the factory should still function
                    // and destruct without issues:

                    for (int32_t i = 0; i < 10; ++ i)
                    {
                        sql_connection_factory::connection & c = sqf.acquire ("test_conn");
                        if (i & 0x01) sqf.release (c);
                    }
                }
                app.stop ();
            }
        }
    }
}

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
