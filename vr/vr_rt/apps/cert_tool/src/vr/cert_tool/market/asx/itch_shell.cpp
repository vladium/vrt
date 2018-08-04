
#include "vr/cert_tool/market/asx/itch_shell.h"

#include "vr/cert_tool/market/asx/itch_session.h"
#include "vr/str_hash.h"
#include "vr/util/getline_loop.h"
#include "vr/util/logging.h"
#include "vr/util/parse.h"

#include <boost/regex.hpp>

#include <iostream>

//----------------------------------------------------------------------------
namespace vr
{
namespace market
{
namespace ASX
{
//............................................................................
//............................................................................
namespace
{

boost::regex const g_cmd_re { R"((?<verb>\w+)\s*(\((?<args>[^)]*)\))?)" }; // <verb> [(<args>)]

bool
parse_cmd (std::string const & cmd, std::string & /* out */verb, std::string & /* out */otk, arg_map & /* out */args)
{
    boost::smatch m { };
    if (VR_LIKELY (boost::regex_match (cmd, m, g_cmd_re)))
    {
        verb.assign (m ["verb"].first, m ["verb"].second);

        if (m ["args"].matched)
        {
            std::string const arg_str { m ["args"].first, m ["args"].second };

            if (! arg_str.empty ())
            {
                string_vector const nv_list = util::split (arg_str, ", ");
                for (std::string const & nv : nv_list)
                {
                    string_vector const ss = util::split (nv, "=");
                    if ((ss.size () != 2) || ss [0].empty ())
                        return false;

                    DLOG_trace3 << "  [" << ss [0] << "] = [" << ss [1] << ']';
                    args.emplace (ss [0], ss [1]);
                }
            }
        }

        return true;
    }

    return false;
}

} // end of anonymous
//............................................................................
//............................................................................

itch_shell::itch_shell ()
{
    dep (m_session) = "session"; // TODO smth less ambiguous
}
//............................................................................

void
itch_shell::run (std::string const & prompt)
{
    util::getline_loop gll { prompt };
    std::string line { };

    while (true)
    {
        if (gll.next (line))
        {
            assert_condition (! line.empty ()); // 'getline_loop' guarantee
            string_vector const cmds = util::split (line, ";");

            for (std::string const & cmd : cmds)
            {
                if (cmd.empty ())
                    continue;

                if (cmd == "quit")
                    return;

                std::string verb { };
                std::string otk { };
                arg_map args { };

                if (! parse_cmd (cmd, verb, otk, args))
                    std::cout << "*** could not parse command ***" << std::endl;
                else
                {
                    switch (str_hash_32 (verb))
                    {
                        case "ls"_hash:
                        case  "L"_hash:
                        {
                            if (VR_UNLIKELY (args.empty ()))
                                std::cout << "*** ls|L (...) ***" << std::endl;
                            else
                            {
                                std::stringstream out;
                                int32_t const rc = m_session->list (std::move (args), out); // note: last use of' args

                                std::cout << out.str () << std::endl;
                                std::cout << "    " << rc << " instrument(s)" << std::endl;
                            }
                        }
                        break;


                        case "qbook"_hash:
                        case    "Q"_hash:
                        {
                            if (VR_UNLIKELY (args.empty ()))
                                std::cout << "*** qbook|Q (...) ***" << std::endl;
                            else
                            {
                                std::stringstream out;
                                m_session->qbook (std::move (args), out); // note: last use of' args

                                std::cout << out.str () << std::endl;
                            }
                        }
                        break;


                        case "obook"_hash:
                        case    "O"_hash:
                        {
                            if (VR_UNLIKELY (args.empty ()))
                                std::cout << "*** obook|O (...) ***" << std::endl;
                            else
                            {
                                std::stringstream out;
                                int32_t const rc = m_session->obook (std::move (args), out); // note: last use of' args

                                std::cout << out.str () << std::endl;
                                std::cout << "    " << rc << " orders(s)" << std::endl;
                            }
                        }
                        break;



                        default: std::cout << "*** invalid command verb '" << verb << "' ***" << std::endl;

                    } // end of switch
                }
            }
        }
    }
}

} // and of 'ASX'
} // end of 'market'
} // end of namespace
//----------------------------------------------------------------------------
