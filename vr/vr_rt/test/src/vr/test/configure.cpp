
#include "vr/test/configure.h"

#include "vr/io/dao/object_DAO.h"
#include "vr/io/sql/sql_connection_factory.h"
#include "vr/market/ref/asx/ref_data.h"
#include "vr/market/rt/cfg/agent_cfg.h"
#include "vr/rt/cfg/app_cfg.h"
#include "vr/rt/cfg/resources.h"
#include "vr/settings.h"

#include "vr/test/utility.h"

//----------------------------------------------------------------------------
namespace vr
{
namespace test
{
using namespace io;
using namespace market;
using namespace market::ASX;
using namespace rt;

//............................................................................

void
configure_app_ref_data (util::di::container & app, string_vector const & symbols)
{
    settings cfg
    {
        { "symbols", {
            { "dummy_A", {
                "/dummy_S", {
                    { "instruments", symbols }
                }
            }}
        }}
        ,
        { "sql", {
            { "sql.ro", {
                { "mode",   "ro" },
                { "cache",  "shared" },
                { "pool_size",  2 },
                { "on_open", {
                    "PRAGMA read_uncommitted=true;"
                }},
                { "db", rt::resolve_as_uri ("asx/ref.equity.db").native () }
            }}
        }}
    };

    LOG_trace1 << "using app config:\n" << print (cfg);

    app.configure ()
        ("config",      new app_cfg { cfg })
        ("ref_data",    new ref_data { { } })
        ("agents",      new agent_cfg { "/symbols" })
        ("DAO",         new object_DAO { { { "cfg.ro", "sql.ro" } } })
        ("sql",         new sql_connection_factory { { { "sql.ro", cfg ["sql"]["sql.ro"] } } })
    ;
}

} // end of 'test'
} // end of namespace
//----------------------------------------------------------------------------
