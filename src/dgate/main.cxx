#include <iostream>
#include <ev++.h>
#include <memory>
#include "common/lmdb++.h"
#include "ircddb/IRCDDBApp.h"
#include "ircddb/IRCMessage.h"

using namespace std;

int main()
{
	auto env_ = lmdb::env::create();
	std::shared_ptr<lmdb::env> env = std::make_shared<lmdb::env>(std::move(env_));

	env->set_mapsize(32UL * 1024UL * 1024UL); // 32MB
	env->set_max_dbs(3);
	env->open("./test.mdb/");

	auto cs_rptr_dbi = std::make_shared<lmdb::dbi>();
	auto zone_ip4 = std::make_shared<lmdb::dbi>();
	auto zone_ip6 = std::make_shared<lmdb::dbi>();

	// Initialize databases
	{
		auto wtxn = lmdb::txn::begin(*env);
		*cs_rptr_dbi = lmdb::dbi::open(wtxn, "cs_rptr_dbi", MDB_CREATE);
		*zone_ip4 = lmdb::dbi::open(wtxn, "zone_ip4", MDB_CREATE);
		*zone_ip6 = lmdb::dbi::open(wtxn, "zone_ip6", MDB_CREATE);

		wtxn.commit();
	}

	IRCDDBApp app("KO6JXH", {{
		"ircv4.openquad.net",
		"",
		9007,
		AF_INET
	}}, "");

	app.run();
}

