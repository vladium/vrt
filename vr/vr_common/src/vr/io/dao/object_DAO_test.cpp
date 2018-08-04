
#include "vr/io/dao/object_DAO.h"
#include "vr/io/files.h"
#include "vr/io/sql/sql_connection_factory.h"
#include "vr/meta/structs.h"
#include "vr/settings.h"
#include "vr/tags.h"
#include "vr/util/di/container.h"

#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
//............................................................................

VR_META_TAG (key2);
VR_META_TAG (state2);

VR_META_COMPACT_STRUCT (test_meta_object,   // int32_t key

    (int32_t,               key)
    (std::string,           state)
    (std::vector<int32_t>,  user_data)

); // end of class

VR_META_COMPACT_STRUCT (test_meta_object2,  // std::string key

    (std::string,           key2)
    (std::string,           state2)

); // end of class
//............................................................................
//............................................................................
// TODO
// - MT safety

TEST (object_DAO, populate_single_type_daily_validity)
{
    fs::path const out_dir { test::unique_test_path () };
    io::create_dirs (out_dir);

    LOG_info << "creating test db in " << print (out_dir);

    fs::path const db_file { out_dir / "db" };

    settings const sql_cfg_ro
    {
        { "mode", "ro" },
        { "cache", "shared" },
        { "db", db_file.string () },
        /* don't cause ro connections to be opened before the db is created { "pool_size", 2 }, */
        { "on_open",
            {
                "PRAGMA read_uncommitted=true;"
            }
        }
    };

    settings const sql_cfg_rwc
    {
        { "mode", "rwc" },
        { "cache", "private" },
        { "db", db_file.string () },
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

    util::date_t const start = util::current_date_local ();
    int32_t const ID = 123;

    {
        util::di::container app { join_as_name ("APP", test::current_test_name ()) };

        app.configure ()
            ("sql", new sql_connection_factory { { { "sql.ro", sql_cfg_ro }, { "sql.rwc", sql_cfg_rwc } } })
            ("DAO", new object_DAO { { { "cfg.ro", "sql.ro" }, { "cfg.rw", "sql.rwc" } } })
        ;

        app.start ();
        {
            object_DAO & dao = app ["DAO"];

            ASSERT_FALSE (dao.read_only ());

            {
                auto & ifc = dao.rw_ifc<test_meta_object> (); // r/w

                auto tx { ifc.tx_begin () }; // do all of the write I/O within one transaction

                test_meta_object obj { };
                {
                    obj.key () = ID;
                    obj.state () = "";
                    obj.user_data ().push_back (333);
                }

                // persist some versions, each at multiple consecutive dates:

                util::date_t d = start;
                for (int32_t i =/* ! */0; i < 30; ++ i, d += gd::days { 1 })
                {
                    obj.state () = join_as_name<'.'> ("S", util::date_as_int (d), i);

                    const bool rc = ifc.persist_as_of (d, obj);
                    ASSERT_TRUE (rc) << "[" << util::date_as_int (d) << "] wrong return code (" << rc << ") for persisting_obj (obj: " << obj << ')';
                }

                tx.commit ();
            }
            {
                auto & ifc = dao.ro_ifc<test_meta_object> (); // ro

                auto tx { ifc.tx_begin () }; // do all of the read I/O within one transaction

                // query each 'vb' date, there should be an object version:

                util::date_t d = start;
                std::string state = "";

                for (int32_t i = /* ! */0; i < 30; ++ i, d += gd::days { 1 })
                {
                    optional<test_meta_object> const obj_result = ifc.find_as_of (d, ID);
                    ASSERT_TRUE (obj_result) << "find failed for [" << util::date_as_int (d) << ']';

                    test_meta_object const & obj = obj_result.get ();

                    // 'state' value should follow a pattern established by the first block:

                    state = join_as_name<'.'> ("S", util::date_as_int (d), i);

                    ASSERT_EQ (obj.key (), ID);
                    EXPECT_EQ (obj.state (), state);
                    EXPECT_TRUE ((obj.user_data ().size () == 1) && (obj.user_data ()[0] == 333));
                }

                // query before validity start, should get NULL:
                {
                    optional<test_meta_object> const obj_result = ifc.find_as_of (start - gd::days (1), ID);
                    ASSERT_FALSE (obj_result);
                }

                // query in the far future, should get the last version:
                {
                    optional<test_meta_object> const obj_result_last = ifc.find_as_of (d - gd::days (1), ID); // last version
                    ASSERT_TRUE (obj_result_last);
                    optional<test_meta_object> const obj_result_fut = ifc.find_as_of (d + gd::days (1000), ID); // future
                    ASSERT_TRUE (obj_result_fut);

                    EXPECT_EQ (obj_result_last->key (), obj_result_fut->key ());
                    EXPECT_EQ (obj_result_last->state (), obj_result_fut->state ());
                }

                tx.commit ();
            }
        }
        app.stop ();
    }

    // shut down 'app' and create another to verify that the data has been committed:

    {
        util::di::container app { join_as_name ("APP", test::current_test_name ()) };

        app.configure ()
            ("sql", new sql_connection_factory { { { "sql.ro", sql_cfg_ro }, { "sql.rwc", sql_cfg_rwc } } })
            ("DAO", new object_DAO { { { "cfg.ro", "sql.ro" }, { "cfg.rw", "sql.rwc" } } })
        ;

        app.start ();
        {
            object_DAO & dao = app ["DAO"];

            {
                auto & ifc = dao.ro_ifc<test_meta_object> (); // ro

                auto tx { ifc.tx_begin () }; // do all of the read I/O within one transaction

                // query each 'vb' date, there should be an object version:

                util::date_t d = start;
                std::string state = "";

                for (int32_t i = /* ! */0; i < 30; ++ i, d += gd::days { 1 })
                {
                    optional<test_meta_object> const obj_result = ifc.find_as_of (d, ID);
                    ASSERT_TRUE (obj_result) << "find failed for [" << util::date_as_int (d) << ']';

                    test_meta_object const & obj = obj_result.get ();

                    // 'state' value should follow a pattern established by the first block:

                    state = join_as_name<'.'> ("S", util::date_as_int (d), i);

                    ASSERT_EQ (obj.key (), ID);
                    EXPECT_EQ (obj.state (), state);
                    EXPECT_TRUE ((obj.user_data ().size () == 1) && (obj.user_data ()[0] == 333));
                }

                // query before validity start, should get NULL:
                {
                    optional<test_meta_object> const obj_result = ifc.find_as_of (start - gd::days (1), ID);
                    ASSERT_FALSE (obj_result);
                }

                // query in the far future, should get the last version:
                {
                    optional<test_meta_object> const obj_result_last = ifc.find_as_of (d - gd::days (1), ID); // last version
                    ASSERT_TRUE (obj_result_last);
                    optional<test_meta_object> const obj_result_fut = ifc.find_as_of (d + gd::days (1000), ID); // future
                    ASSERT_TRUE (obj_result_fut);

                    EXPECT_EQ (obj_result_last->key (), obj_result_fut->key ());
                    EXPECT_EQ (obj_result_last->state (), obj_result_fut->state ());
                }

                tx.commit ();
            }
        }
        app.stop ();
    }
}
//............................................................................

TEST (object_DAO, populate_single_type_multiday_validity)
{
    fs::path const out_dir { test::unique_test_path () };
    io::create_dirs (out_dir);

    LOG_info << "creating test db in " << print (out_dir);

    fs::path const db_file { out_dir / "db" };

    settings const sql_cfg_ro
    {
        { "mode", "ro" },
        { "cache", "shared" },
        { "db", db_file.string () },
        /* don't cause ro connections to be opened before the db is created { "pool_size", 2 }, */
        { "on_open",
            {
                "PRAGMA read_uncommitted=true;"
            }
        }
    };

    settings const sql_cfg_rwc
    {
        { "mode", "rwc" },
        { "cache", "private" },
        { "db", db_file.string () },
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

    util::date_t const start = util::current_date_local ();
    int32_t const ID = 123;

    {
        util::di::container app { join_as_name ("APP", test::current_test_name ()) };

        app.configure ()
            ("sql", new sql_connection_factory { { { "sql.ro", sql_cfg_ro }, { "sql.rwc", sql_cfg_rwc } } })
            ("DAO", new object_DAO { { { "cfg.ro", "sql.ro" }, { "cfg.rw", "sql.rwc" } } })
        ;

        app.start ();
        {
            object_DAO & dao = app ["DAO"];

            ASSERT_FALSE (dao.read_only ());

            {
                auto & ifc = dao.rw_ifc<test_meta_object> (); // r/w

                auto tx { ifc.tx_begin () }; // do all of the write I/O within one transaction

                test_meta_object obj { };
                {
                    obj.key () = ID;
                    obj.state () = "";
                    obj.user_data ().push_back (333);
                }

                // persist some versions, each at multiple consecutive dates:

                util::date_t d = start;
                for (int32_t i =/* ! */0; i < 30; ++ i, d += gd::days { 1 })
                {
                    bool const change_obj = (! (i % 3));

                    if (change_obj)
                        obj.state () = join_as_name<'.'> ("S", util::date_as_int (d), i);

                    const bool rc = ifc.persist_as_of (d, obj);
                    ASSERT_EQ (rc, change_obj) << "[" << util::date_as_int (d) << "] wrong return code (" << rc << ") for change_obj " << change_obj << " (obj: " << obj << ')';
                }

                tx.commit ();
            }
            {
                auto & ifc = dao.ro_ifc<test_meta_object> (); // ro

                auto tx { ifc.tx_begin () }; // do all of the read I/O within one transaction

                // query each 'vb' date, there should be an object version:

                util::date_t d = start;
                std::string state = "";

                for (int32_t i = /* ! */0; i < 30; ++ i, d += gd::days { 1 })
                {
                    optional<test_meta_object> const obj_result = ifc.find_as_of (d, ID);
                    ASSERT_TRUE (obj_result) << "find failed for [" << util::date_as_int (d) << ']';

                    test_meta_object const & obj = obj_result.get ();

                    // 'state' value should follow a pattern established by the first block:
                    // a new version starts when ((i + 1) % 3) is zero:

                    if (! (i % 3))
                        state = join_as_name<'.'> ("S", util::date_as_int (d), i);

                    ASSERT_EQ (obj.key (), ID);
                    EXPECT_EQ (obj.state (), state);
                    EXPECT_TRUE ((obj.user_data ().size () == 1) && (obj.user_data ()[0] == 333));
                }

                // query before validity start, should get NULL:
                {
                    optional<test_meta_object> const obj_result = ifc.find_as_of (start - gd::days (1), ID);
                    ASSERT_FALSE (obj_result);
                }

                // query in the far future, should get the last version:
                {
                    optional<test_meta_object> const obj_result_last = ifc.find_as_of (d - gd::days (1), ID); // last version
                    ASSERT_TRUE (obj_result_last);
                    optional<test_meta_object> const obj_result_fut = ifc.find_as_of (d + gd::days (1000), ID); // future
                    ASSERT_TRUE (obj_result_fut);

                    EXPECT_EQ (obj_result_last->key (), obj_result_fut->key ());
                    EXPECT_EQ (obj_result_last->state (), obj_result_fut->state ());
                }

                tx.commit ();
            }
        }
        app.stop ();
    }

    // shut down 'app' and create another to verify that the data has been committed:

    {
        util::di::container app { join_as_name ("APP", test::current_test_name ()) };

        app.configure ()
            ("sql", new sql_connection_factory { { { "sql.ro", sql_cfg_ro }, { "sql.rwc", sql_cfg_rwc } } })
            ("DAO", new object_DAO { { { "cfg.ro", "sql.ro" }, { "cfg.rw", "sql.rwc" } } })
        ;

        app.start ();
        {
            object_DAO & dao = app ["DAO"];

            {
                auto & ifc = dao.ro_ifc<test_meta_object> (); // ro

                auto tx { ifc.tx_begin () }; // do all of the read I/O within one transaction

                // query each 'vb' date, there should be an object version:

                util::date_t d = start;
                std::string state = "";

                for (int32_t i = /* ! */0; i < 30; ++ i, d += gd::days { 1 })
                {
                    optional<test_meta_object> const obj_result = ifc.find_as_of (d, ID);
                    ASSERT_TRUE (obj_result) << "find failed for [" << util::date_as_int (d) << ']';

                    test_meta_object const & obj = obj_result.get ();

                    // 'state' value should follow a pattern established by the first block:
                    // a new version starts when ((i + 1) % 3) is zero:

                    if (! (i % 3))
                        state = join_as_name<'.'> ("S", util::date_as_int (d), i);

                    ASSERT_EQ (obj.key (), ID);
                    EXPECT_EQ (obj.state (), state);
                    EXPECT_TRUE ((obj.user_data ().size () == 1) && (obj.user_data ()[0] == 333));
                }

                // query before validity start, should get NULL:
                {
                    optional<test_meta_object> const obj_result = ifc.find_as_of (start - gd::days (1), ID);
                    ASSERT_FALSE (obj_result);
                }

                // query in the far future, should get the last version:
                {
                    optional<test_meta_object> const obj_result_last = ifc.find_as_of (d - gd::days (1), ID); // last version
                    ASSERT_TRUE (obj_result_last);
                    optional<test_meta_object> const obj_result_fut = ifc.find_as_of (d + gd::days (1000), ID); // future
                    ASSERT_TRUE (obj_result_fut);

                    EXPECT_EQ (obj_result_last->key (), obj_result_fut->key ());
                    EXPECT_EQ (obj_result_last->state (), obj_result_fut->state ());
                }

                tx.commit ();
            }
        }
        app.stop ();
    }
}
//............................................................................

TEST (object_DAO, populate_two_types)
{
    fs::path const out_dir { test::unique_test_path () };
    io::create_dirs (out_dir);

    LOG_info << "creating test db in " << print (out_dir);

    fs::path const db_file { out_dir / "db" };

    settings const sql_cfg_ro
    {
        { "mode", "ro" },
        { "cache", "shared" },
        { "db", db_file.string () },
        /* don't cause ro connections to be opened before the db is created { "pool_size", 2 }, */
        { "on_open",
            {
                "PRAGMA read_uncommitted=true;"
            }
        }
    };

    settings const sql_cfg_rwc
    {
        { "mode", "rwc" },
        { "cache", "private" },
        { "db", db_file.string () },
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
            ("sql", new sql_connection_factory { { { "sql.ro", sql_cfg_ro }, { "sql.rwc", sql_cfg_rwc } } })
            ("DAO", new object_DAO { { { "cfg.ro", "sql.ro" }, { "cfg.rw", "sql.rwc" } } })
        ;

        app.start ();
        {
            object_DAO & dao = app ["DAO"];

            ASSERT_FALSE (dao.read_only ());

            util::date_t const start = util::current_date_local ();
            int32_t const ID = 123;
            std::string const ID2 = "APPL";

            {
                auto & ifc = dao.rw_ifc<test_meta_object> (); // r/w
                auto & ifc2 = dao.rw_ifc<test_meta_object2> (); // r/w

                // [note: leave autocommit mode on to avoid locking the db]

                test_meta_object obj { };
                {
                    obj.key () = ID;
                    obj.state () = "";
                    obj.user_data ().push_back (333);
                }

                test_meta_object2 obj2 { };
                {
                    obj2.key2 () = ID2;
                    obj2.state2 () = "";
                }

                // persist some versions, each at multiple consecutive dates:

                util::date_t d = start;
                for (int32_t i =/* ! */0; i < 30; ++ i, d += gd::days { 1 })
                {
                    bool const change_obj = (! (i % 3));

                    if (change_obj)
                    {
                        obj.state () = join_as_name<'.'> ("S", util::date_as_int (d), i);
                        obj2.state2 () = obj.state ();
                    }

                    const bool rc = ifc.persist_as_of (d, obj);
                    ASSERT_EQ (rc, change_obj) << "[" << util::date_as_int (d) << "] wrong return code (" << rc << ") for change_obj " << change_obj << " (obj: " << obj << ')';

                    const bool rc2 = ifc2.persist_as_of (d, obj2);
                    ASSERT_EQ (rc2, change_obj) << "[" << util::date_as_int (d) << "] wrong return code (" << rc << ") for change_obj " << change_obj << " (obj2: " << obj2 << ')';
                }

                // for 'test_meta_object2' only, stop the lifeline:

                const bool rc2 = ifc2.delete_as_of (d, ID2);
                ASSERT_TRUE (rc2) << "[" << util::date_as_int (d) << "] wrong return code (" << rc2 << ") for deletion";
            }

            {
                auto & ifc = dao.ro_ifc<test_meta_object> (); // ro
                auto & ifc2 = dao.ro_ifc<test_meta_object2> (); // ro

                auto tx { ifc.tx_begin () }; // do all of the read I/O within one transaction
                auto tx2 { ifc2.tx_begin () }; // do all of the read I/O within one transaction

                // query each 'vb' date, there should be an object version:

                util::date_t d = start;
                std::string state = "";

                for (int32_t i = /* ! */0; i < 30; ++ i, d += gd::days { 1 })
                {
                    optional<test_meta_object> const obj_result = ifc.find_as_of (d, ID);
                    ASSERT_TRUE (obj_result) << "find failed for [" << util::date_as_int (d) << ']';

                    optional<test_meta_object2> const obj2_result = ifc2.find_as_of (d, ID2);
                    ASSERT_TRUE (obj2_result) << "find failed for [" << util::date_as_int (d) << ']';


                    test_meta_object const & obj = obj_result.get ();
                    test_meta_object2 const & obj2 = obj2_result.get ();

                    // 'state' value should follow a pattern established by the first block:
                    // a new version starts when ((i + 1) % 3) is zero:

                    if (! (i % 3))
                        state = join_as_name<'.'> ("S", util::date_as_int (d), i);

                    ASSERT_EQ (obj.key (), ID);
                    ASSERT_EQ (obj2.key2 (), ID2);

                    EXPECT_EQ (obj.state (), state);
                    EXPECT_EQ (obj2.state2 (), state);

                    EXPECT_TRUE ((obj.user_data ().size () == 1) && (obj.user_data ()[0] == 333));
                }

                // query before validity start, should get NULL:
                {
                    optional<test_meta_object> const obj_result = ifc.find_as_of (start - gd::days (1), ID);
                    ASSERT_FALSE (obj_result);

                    optional<test_meta_object2> const obj2_result = ifc2.find_as_of (start - gd::days (1), ID2);
                    ASSERT_FALSE (obj2_result);
                }

                // query in the far future, should get the last version for 'test_meta_object' and NULL for 'test_meta_object2':
                {
                    optional<test_meta_object> const obj_result_last = ifc.find_as_of (d - gd::days (1), ID); // last version
                    ASSERT_TRUE (obj_result_last);
                    optional<test_meta_object> const obj_result_fut = ifc.find_as_of (d + gd::days (1000), ID); // future
                    ASSERT_TRUE (obj_result_fut);

                    EXPECT_EQ (obj_result_last->key (), obj_result_fut->key ());
                    EXPECT_EQ (obj_result_last->state (), obj_result_fut->state ());


                    optional<test_meta_object2> const obj2_result_fut = ifc2.find_as_of (d + gd::days (1000), ID2); // future
                    ASSERT_FALSE (obj2_result_fut);
                }

                tx2.commit ();
                tx.commit ();
            }
        }
        app.stop ();
    }
}

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
