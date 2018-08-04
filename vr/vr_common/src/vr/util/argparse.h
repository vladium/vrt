#pragma once

#include "vr/sys/os.h" // proc_tty_cols()
#include "vr/version.h"

#include <exception>
#include <iostream>

#include <boost/preprocessor/cat.hpp>
#include <boost/preprocessor/variadic/size.hpp>

#include <boost/program_options/option.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/positional_options.hpp>
#include <boost/program_options/variables_map.hpp>
namespace bpopt = boost::program_options;

//----------------------------------------------------------------------------

#define vr_ARGPARSE_3(av, opts, popts) \
    bpopt::variables_map args; \
    try \
    { \
        bpopt::store (bpopt::command_line_parser (av).options (opts).positional (popts).run (), args); \
        if (args.count ("help")) \
        { \
            std::cout << std::endl << opts << std::endl; \
            return 0; \
        } \
        if (args.count ("version")) \
        { \
            std::cout << VR_BUILD_VERSION << std::endl; \
            return 0; \
        } \
        bpopt::notify (args); \
     } \
     catch (std::exception const & e) \
     { \
         std::cerr << std::endl << e.what () << "\n\n" << opts << std::endl; \
         return 1; \
     } \
     /* */

#define vr_ARGPARSE_4(ac, av, opts, popts) \
    bpopt::variables_map args; \
    try \
    { \
        bpopt::store (bpopt::command_line_parser (ac, av).options (opts).positional (popts).run (), args); \
        if (args.count ("help")) \
        { \
            std::cout << std::endl << opts << std::endl; \
            return 0; \
        } \
        if (args.count ("version")) \
        { \
            std::cout << VR_BUILD_VERSION << std::endl; \
            return 0; \
        } \
        bpopt::notify (args); \
     } \
     catch (std::exception const & e) \
     { \
         std::cerr << std::endl << e.what () << "\n\n" << opts << std::endl; \
         return 1; \
     } \
     /* */

//............................................................................

#define VR_ARGPARSE(...) \
    BOOST_PP_CAT (vr_ARGPARSE_, BOOST_PP_VARIADIC_SIZE (__VA_ARGS__)) (__VA_ARGS__) \
    /* */

#define VR_OP_ARGPARSE(ac, av, opts, popts) \
    bpopt::variables_map args; \
    std::vector<std::string> op_args; \
    try \
    { \
        bpopt::parsed_options po { bpopt::command_line_parser (ac, av).options (opts).positional (popts).allow_unregistered ().run () }; \
        bpopt::store (po, args); \
        if (args.count ("help")) \
        { \
            std::cout << std::endl << opts << std::endl; \
            return 0; \
        } \
        if (args.count ("version")) \
        { \
            std::cout << VR_BUILD_VERSION << std::endl; \
            return 0; \
        } \
        bpopt::notify (args); \
        op_args = bpopt::collect_unrecognized (po.options, bpopt::include_positional); \
        op_args.erase (op_args.begin ()); \
     } \
     catch (std::exception const & e) \
     { \
         std::cerr << std::endl << e.what () << "\n\n" << opts << std::endl; \
         return 1; \
     } \
     /* */

//----------------------------------------------------------------------------
