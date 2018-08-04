
#include "vr/io/files.h"

#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace io
{
//............................................................................

TEST (find_path_recursive, single_segment)
{
    fs::path const search_root { test::unique_test_path () };
    create_dirs (search_root);

    // create a dir sub-tree of known structure:

    create_dirs (search_root / "a");
    create_dirs (search_root / "b");
    create_dirs (search_root / "x" / "c" / "y" );
    create_dirs (search_root / "x" / "c" / "b" );
    create_dirs (search_root / "c");

    // search for "a" will return one of two possibilities:

    optional<fs::path> const match = find_path_recursive ("b", search_root);
    ASSERT_TRUE (match);

    fs::path const & p = * match;
    EXPECT_TRUE ((p == search_root / "b") || (p == search_root / "x" / "c" / "b"));
}

TEST (find_path_recursive, multi_segment)
{
    fs::path const search_root { test::unique_test_path () };
    create_dirs (search_root);

    // create a dir sub-tree of known structure:

    create_dirs (search_root / "a");
    create_dirs (search_root / "b");
    create_dirs (search_root / "c" / "c" / "y" / "c" );
    create_dirs (search_root / "c" / "c" / "b" / "c" );

    // search for "c/a" will return only one possibility:

    optional<fs::path> const match = find_path_recursive ("c/b", search_root);
    ASSERT_TRUE (match);

    fs::path const & p = * match;
    EXPECT_TRUE (p == search_root / "c" / "c" / "b");
}

TEST (find_path_recursive, absolute)
{
    fs::path const search_root_A { test::unique_test_path () };
    create_dirs (search_root_A);
    fs::path const search_root_B { test::unique_test_path () };
    create_dirs (search_root_B);

    create_dirs (search_root_A / "a");
    create_dirs (search_root_B / "a");

    // search for "/...<search_root_B>/a" from root A will ignore the search root:

    fs::path const abs_a = absolute_path ("a", search_root_B);

    optional<fs::path> const match = find_path_recursive (abs_a, search_root_A);
    ASSERT_TRUE (match);

    fs::path const & p = * match;
    EXPECT_TRUE (p == abs_a);
}

} // end of 'io'
} // end of namespace
//----------------------------------------------------------------------------
