
#include "vr/cert_tool/market/asx/ouch_shell.h"

#include "vr/cert_tool/market/asx/ouch_session.h"
#include "vr/str_hash.h"
#include "vr/util/getline_loop.h"
#include "vr/util/logging.h"
#include "vr/util/parse.h"

#include <boost/regex.hpp>

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

boost::regex const g_cmd_re { R"((?<verb>\w+)\s*(?<otk>[^\s()]+)?\s*(\((?<args>[^)]*)\))?)" }; // <verb> [<otk>] [(<args>)]

bool
parse_cmd (std::string const & cmd, std::string & /* out */verb, std::string & /* out */otk, arg_map & /* out */args)
{
    boost::smatch m { };
    if (VR_LIKELY (boost::regex_match (cmd, m, g_cmd_re)))
    {
        verb.assign (m ["verb"].first, m ["verb"].second);

        if (m ["otk"].matched)
        {
            otk.assign (m ["otk"].first, m ["otk"].second);
        }

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

ouch_shell::ouch_shell ()
{
    dep (m_session) = "session";
}
//............................................................................

void
ouch_shell::run (std::string const & prompt)
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
                    LOG_warn << "*** could not parse command ***";
                else
                {
                    switch (str_hash_32 (verb))
                    {
                        case "submit"_hash:
                        case      "N"_hash:
                        {
                            if (VR_UNLIKELY (args.empty ()))
                                LOG_warn << "*** submit|N (...) ***";
                            else
                            {
                                auto const otk = m_session->submit (std::move (args)); // note: last use of' args
                                if (otk.size ()) LOG_info << "otk: " << otk;
                            }
                        }
                        break;


                        case "replace"_hash:
                        case       "R"_hash:
                        {
                            if (VR_UNLIKELY (otk.empty ()))
                                LOG_warn << "*** replace|R <otk> (...) ***";
                            else
                            {
                                auto const new_otk = m_session->replace (otk, std::move (args)); // note: last use of' args
                                if (new_otk.size ()) LOG_info << "new otk: " << new_otk;
                            }
                        }
                        break;


                        case "cancel"_hash:
                        case      "C"_hash:
                        {
                            if (VR_UNLIKELY (otk.empty ()))
                                LOG_warn << "*** cancel|C <otk> ***";
                            else
                            {
                                m_session->cancel (otk, std::move (args));
                            }
                        }
                        break;


                        case "book"_hash:
                        {
                            std::stringstream ss;
                            int32_t const oc = m_session->list_book (ss);

                            LOG_info << oc << " order(s):\r\n" << ss.str ();
                        }
                        break;


                        default: LOG_warn << "*** invalid command verb '" << verb << "' ***";

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
