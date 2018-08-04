
#include "vr/data/dataframe.h"

#include "vr/test/utility.h"

#include <map>

//----------------------------------------------------------------------------
namespace vr
{
namespace data
{
//............................................................................
//............................................................................
namespace
{

std::vector<attribute> const &
test_attrs ()
{
    static std::vector<attribute> const g_attrs { attribute::parse ("A_cat: {A, B, C, D};"
                                                                    "A_ts: time;"
                                                                    "A_i4: i4;"
                                                                    "A_i8: i8;"
                                                                    "A_f4: f4;"
                                                                    "A_f8: f8;") };

    return g_attrs;
}

std::vector<attribute> const &
test_attrs_add_row ()
{
    static std::vector<attribute> const g_attrs { attribute::parse ("A_cat: {A, B, C, D};"
                                                                    "A_ts: time;"
                                                                    "A_i4: i4;"
                                                                    "A_i8:   i8;"
                                                                    "A_f4:   f4;"
                                                                    "A_i8_2: i8;" // for testing add_row() array-of-tuple args
                                                                    "A_f4_2: f4;"
                                                                    "A_f8, A_f8_2: f8;") }; // for testing add_row() array args

    return g_attrs;
}
//............................................................................

struct check_structure final
{
    template<atype::enum_t ATYPE>
    void operator() (atype_<ATYPE>, dataframe const & df, width_type const col) const
    {
        using data_type             = typename atype::traits<ATYPE>::type;

        data_type const * VR_RESTRICT col_data = df.at<data_type> (col); // this checks addr ranges in debug build

        if (uintptr (col_data) % alignof (data_type))
            throw_x (check_failure, "col " + string_cast (col) + ": bad alignment for {" + util::class_name<data_type> () + "} @" + print (col_data));

        if (! m_col_starts.emplace (uintptr (col_data), sizeof (data_type)).second)
            throw_x (check_failure, "col " + string_cast (col) + ": starting addr overlaps with another column");

        if (signed_cast (m_col_starts.size ()) == df.col_count ()) // visited all columns
        {
            // check for col memory overlaps:

            std_uintptr_t addr_prev { };
            int32_t size_prev { };

            const int32_t row_capacity = df.row_capacity ();

            for (auto const & kv : m_col_starts)
            {
                if (VR_LIKELY (size_prev != 0))
                {
                    check_le (addr_prev + size_prev * row_capacity, kv.first);
                }

                addr_prev = kv.first;
                size_prev = kv.second;
            }
        }
    }

    mutable std::map<std_uintptr_t, int32_t> m_col_starts { }; // ordered so we can test for overlap in O(N)

}; // end of class
//............................................................................

struct write_data final
{
    template<atype::enum_t ATYPE>
    void operator() (atype_<ATYPE>, dataframe & df, width_type const col) const
    {
        using data_type             = typename atype::traits<ATYPE>::type;
        using data_int_type         = typename util::signed_type_of_size<sizeof (data_type)>::type;

        dataframe::size_type const i_limit = df.row_count ();
        assert_positive (i_limit);

        data_int_type * VR_RESTRICT col_data = reinterpret_cast<data_int_type *> (df.at<data_type> (col));

        data_int_type rnd { col + test::env::random_seed<data_int_type> () };

        col_data [0] = rnd;
        for (dataframe::size_type i = /* ! */1; i < i_limit; ++ i)
        {
            col_data [i] = test::next_random (rnd);
        }
    }

}; // end of class

struct check_data final
{
    template<atype::enum_t ATYPE>
    void operator() (atype_<ATYPE>, dataframe const & df, width_type const col) const
    {
        using data_type             = typename atype::traits<ATYPE>::type;
        using data_int_type         = typename util::signed_type_of_size<sizeof (data_type)>::type;

        dataframe::size_type const i_limit = df.row_count ();
        assert_positive (i_limit);

        data_int_type const * VR_RESTRICT col_data = reinterpret_cast<data_int_type const *> (df.at<data_type> (col));

        data_int_type rnd = col_data [0];
        for (dataframe::size_type i = /* ! */1; i < i_limit; ++ i)
        {
            test::next_random (rnd);
            check_eq (col_data [i], rnd, col);
        }

        check_eq (col_data, df.at_raw (col));
    }

}; // end of class
//............................................................................

using scenarios         = gt::Types<util::int_<int32_t, 0>, util::int_<int32_t, 1>, util::int_<int32_t, 2> >;

template<typename T> struct dataframe_test: public gt::Test { };
TYPED_TEST_CASE (dataframe_test, scenarios);

} // end of anonymous
//............................................................................
//............................................................................

TYPED_TEST (dataframe_test, construction)
{
    using scenario          = TypeParam; // test parameter

    uint32_t rnd { test::env::random_seed<uint32_t> () };

    for (attr_schema::size_type as_size = 1; as_size < 300; ++ as_size)
    {
        // select some random attr types to make a schema of given width:

        LOG_trace1 << " ------- schema size " << as_size;

        std::vector<attribute> attrs;
        {
            std::vector<attribute> const & choices = test_attrs ();

            for (int32_t i = 0; i < as_size; ++ i)
            {
                switch (scenario::value) // compile-time switch
                {
                    case 0:
                    {
                        attribute const & exemplar = choices [test::next_random (rnd) % choices.size ()];

                        attrs.emplace_back ("A." + string_cast (i), exemplar.type ());
                    }
                    break;

                    case 1: // edge case (all attrs are of the same width)
                    {
                        attrs.emplace_back ("A." + string_cast (i), atype::i4);
                    }
                    break;

                    default: // edge case (all attrs are of the same width)
                    {
                        attrs.emplace_back ("A." + string_cast (i), atype::i8);
                    }
                    break;

                } // end of switch
            }
        }

        for (dataframe::size_type row_capacity = 1; row_capacity <= 16 * 1024; row_capacity <<= 1)
        {
            LOG_trace1 << "df dims: " << row_capacity << 'x' << as_size;

            dataframe const df { row_capacity, attrs };

            ASSERT_EQ (df.row_capacity (), row_capacity);
            ASSERT_EQ (df.col_count (), as_size);
            ASSERT_EQ (df.row_count (), 0);

            df.accept (check_structure { });
        }
    }
}
//............................................................................

TYPED_TEST (dataframe_test, round_trip)
{
    using scenario          = TypeParam; // test parameter

    uint32_t rnd { test::env::random_seed<uint32_t> () };

    for (attr_schema::size_type as_size = 1; as_size < 100; ++ as_size)
    {
        // select some random attr types to make a schema of given width:

        LOG_trace1 << " ------- schema size " << as_size;

        std::vector<attribute> attrs;
        {
            std::vector<attribute> const & choices = test_attrs ();

            for (int32_t i = 0; i < as_size; ++ i)
            {
                switch (scenario::value) // compile-time switch
                {
                    case 0:
                    {
                        attribute const & exemplar = choices [test::next_random (rnd) % choices.size ()];

                        attrs.emplace_back ("A." + string_cast (i), exemplar.type ());
                    }
                    break;

                    case 1: // edge case (all attrs are of the same width)
                    {
                        attrs.emplace_back ("A." + string_cast (i), atype::i4);
                    }
                    break;

                    default: // edge case (all attrs are of the same width)
                    {
                        attrs.emplace_back ("A." + string_cast (i), atype::i8);
                    }
                    break;

                } // end of switch
            }
        }

        for (dataframe::size_type row_capacity = 1; row_capacity <= 16 * 1024; row_capacity <<= 1)
        {
            LOG_trace1 << "df dims: " << row_capacity << 'x' << as_size;

            dataframe df { row_capacity, attrs };

            ASSERT_EQ (df.row_capacity (), row_capacity);
            ASSERT_EQ (df.col_count (), as_size);
            ASSERT_EQ (df.row_count (), 0);

            ASSERT_THROW (df.resize_row_count (2 * row_capacity), invalid_input);
            ASSERT_EQ (df.row_count (), 0);

            df.resize_row_count (row_capacity);
            ASSERT_EQ (df.row_count (), row_capacity);

            df.accept (write_data { });
            df.accept (check_data { }); // note: needs to be a separate visit, to catch data overwriting due to invalid addr overlaps
        }
    }
}
//............................................................................

TEST (dataframe_test, add_row_scalars)
{
    timestamp_t ts { 123 };

    for (dataframe::size_type row_capacity = 1; row_capacity <= 256; row_capacity <<= 1)
    {
        dataframe df { row_capacity, test_attrs_add_row () };

        // fill 'df' via add_row():

        for (dataframe::size_type r = 0; r < row_capacity; ++ r)
        {
            auto const rc = df.add_row (1, ts, 1, 2L, 1.0F, 3L, 2.0F, 2.34, 4.56);
            ASSERT_EQ (rc, r);

            ASSERT_EQ (df.at<timestamp_t> (1)[rc], ts); // data was actually written

            ++ ts;
        }
        ASSERT_THROW (df.add_row<true> (1, ts, 1, 2L, 3L, 2.0F, 1.0F, 2.34, 4.56), out_of_bounds);
    }
}

TEST (dataframe_test, add_row_arrays)
{
    timestamp_t ts { 123 };

    for (dataframe::size_type row_capacity = 1; row_capacity <= 256; row_capacity <<= 1)
    {
        dataframe df { row_capacity, test_attrs_add_row () };

        // fill 'df' via add_row():

        std::array<int64_t, 1> const la { 2L }; // the array extent is 1 but this tests 'T' that is a [const] ref type

        for (dataframe::size_type r = 0; r < row_capacity; ++ r)
        {
            auto const rc = df.add_row (1, ts, 1, la, 1.0F, la, 2.0F, std::array<double, 2> { 2.34, 4.56 });
            ASSERT_EQ (rc, r);

            ASSERT_EQ (df.at<timestamp_t> (1)[rc], ts); // data was actually written

            ++ ts;
        }
        ASSERT_THROW (df.add_row<true> (1, ts, 1, la, 1.0F, la, 2.0F, std::array<double, 2> { 2.34, 4.56 }), out_of_bounds);
    }
}

TEST (dataframe_test, add_row_tuples)
{
    timestamp_t ts { 123 };

    for (dataframe::size_type row_capacity = 1; row_capacity <= 256; row_capacity <<= 1)
    {
        dataframe df { row_capacity, test_attrs_add_row () };

        // fill 'df' via add_row():

        std::tuple<int64_t> const lt { 2L }; // the size is 1 but this tests 'T' that is a [const] ref type

        for (dataframe::size_type r = 0; r < row_capacity; ++ r)
        {
            auto const rc = df.add_row (1, ts, 1, lt, std::make_tuple (1.0F, 3L, 2.0F, 2.34, 4.56));
            ASSERT_EQ (rc, r);

            ASSERT_EQ (df.at<timestamp_t> (1)[rc], ts); // data was actually written

            ++ ts;
        }
        ASSERT_THROW (df.add_row<true> (1, ts, 1, lt, std::make_tuple (1.0F, 3L, 2.0F, 2.34, 4.56)), out_of_bounds);
    }
}

TEST (dataframe_test, add_row_nesting)
{
    timestamp_t ts { 123 };

    for (dataframe::size_type row_capacity = 1; row_capacity <= 256; row_capacity <<= 1)
    {
        dataframe df { row_capacity, test_attrs_add_row () };

        // fill 'df' via add_row():

        std::array<std::tuple<int64_t, float>, 2> const nested { std::make_tuple (1.0F, 2L), std::make_tuple (2.0F, 3L) };
        std::array<double, 2> const da { 2.34, 4.56 };

        for (dataframe::size_type r = 0; r < row_capacity; ++ r)
        {
            auto const rc = df.add_row (1, ts, 1, nested, da);
            ASSERT_EQ (rc, r);

            ASSERT_EQ (df.at<timestamp_t> (1)[rc], ts); // data was actually written

            ++ ts;
        }
        ASSERT_THROW (df.add_row<true> (1, ts, 1, nested, da), out_of_bounds);
    }
}
//............................................................................

TEST (dataframe_test, print)
{
    int64_t rnd = test::env::random_seed<int64_t> ();

    dataframe df { 32 * 1024, "A0: i4; A1: f8; A2: time; A3, A4_long_name: {Q, R, S}; A5: f4; A6: i8; A7: {XYZ, LONG_LABEL};" };

    for (dataframe::size_type rc = 1; rc < df.row_capacity (); rc = 2 * rc + 1)
    {
        df.resize_row_count (rc);

        test::random_fill (df, rnd);

        df.at<int32_t> ("A0")[0] = std::numeric_limits<int32_t>::max ();
        df.at<int64_t> ("A6")[0] = std::numeric_limits<int64_t>::max ();

        df.at<double> ("A1")[0] = std::numeric_limits<double>::max ();
        df.at<float> ("A5")[0] = std::numeric_limits<float>::max ();

        if (rc > 1)
        {
            df.at<int32_t> ("A0")[1] = std::numeric_limits<int32_t>::min ();
            df.at<int64_t> ("A6")[1] = std::numeric_limits<int64_t>::min ();

            df.at<double> ("A1")[1] = - std::numeric_limits<double>::max ();
            df.at<float> ("A5")[1] = - std::numeric_limits<float>::max ();
        }

        LOG_trace1 << "--------------------------------------------------------------------------------------\n" << df;

        std::string const df_str = print (df);
        ASSERT_FALSE (df_str.empty ());
    }
}

} // end of 'data'
} // end of namespace
//----------------------------------------------------------------------------
