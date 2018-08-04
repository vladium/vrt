#pragma once

#include "vr/test/data.h"
#include "vr/test/timing.h"

#include "vr/filesystem.h"
#include "vr/utility.h" // instance_counter
#include "vr/util/random.h"

#include <gtest/gtest.h>
namespace gt   = ::testing;

#include <boost/preprocessor/seq/for_each_product.hpp> // common need for typed tests
#include <boost/preprocessor/seq/enum.hpp> // common need for typed tests

//----------------------------------------------------------------------------
namespace vr
{
namespace test
{
//............................................................................

#if !defined (VR_FULL_TESTS)
#   define VR_FULL_TESTS    0
#endif // VR_FULL_TESTS

// environment variables:

#if !defined (VR_ENV_TEST_SEED)
#   define VR_ENV_TEST_SEED         VR_APP_NAME "_TEST_SEED"
#endif

#if !defined (VR_ENV_TEST_TEMP_CLEAN)
#   define VR_ENV_TEST_TEMP_CLEAN   VR_APP_NAME "_TEST_TEMP_CLEAN"
#endif

#if !defined (VR_ENV_TEST_DATA_ROOT)    // in-repo test artifacts
#   define VR_ENV_TEST_DATA_ROOT    VR_APP_NAME "_TEST_DATA_ROOT"
#endif

#if !defined (VR_ENV_CAP_ROOT)          // externally fetched test artifacts
#   define VR_ENV_CAP_ROOT          VR_APP_NAME "_CAP_ROOT"
#endif

#if !defined (VR_ENV_TEST_NET_IFC)
#   define VR_ENV_TEST_NET_IFC      VR_APP_NAME "_TEST_NET_IFC"
#endif

//............................................................................

template<typename DERIVED>
using instance_counter      = vr::instance_counter<DERIVED>;

//............................................................................

class env final: public gt::Environment
{
    public: // ............................................................

        /**
         * returns a random seed value that varies from test run to test run, but for
         * reproducible runs can also be overridden
         *
         * @return a random seed suitable for 'next_random()' (guaranteed to be non-zero)
         */
        template<typename T>
        static T random_seed ()
        {
            return static_cast<T> (s_random_seed);
        }

    private: // ..............................................................

        // gt::Environment:

        VR_ASSUME_COLD void SetUp () override;
        VR_ASSUME_COLD void TearDown () override;


        static uint64_t s_random_seed;

}; // end of class
//............................................................................

extern std::string
current_test_name ();

extern fs::path
temp_dir ();

extern fs::path const &
data_dir ();

extern fs::path
unique_test_path (std::string const & name_suffix = { });

//............................................................................

extern std::string const &
mcast_ifc ();

//............................................................................

extern int32_t
read_lines (std::istream & in, string_vector & out);

//............................................................................

} // end of 'test'
} // end of namespace
//----------------------------------------------------------------------------
