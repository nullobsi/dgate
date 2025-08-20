//
// d-gate: d-star packet router <https://git.unix.dog/nullobsi/dgate/>
//
// SPDX-FileCopyrightText: 2025 Juan Pablo Zendejas <nullobsi@unix.dog>
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as
// published by the Free Software Foundation, either version 3 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public
// License along with this program. If not, see
// <https://www.gnu.org/licenses/>.
//

#include "common/lmdb++.h"
#include "dgate/app.h"
#include "ircddb/app.h"
#include <ev++.h>
#include <iostream>
#include <memory>

using namespace std;

int main()
{
	auto env_ = lmdb::env::create();
	std::shared_ptr<lmdb::env> env = std::make_shared<lmdb::env>(std::move(env_));

	env->set_mapsize(32UL * 1024UL * 1024UL);// 32MB
	env->set_max_dbs(4);
	env->open("./test.mdb/");

	auto cs_rptr = std::make_shared<lmdb::dbi>();
	auto zone_ip4 = std::make_shared<lmdb::dbi>();
	auto zone_ip6 = std::make_shared<lmdb::dbi>();
	auto zone_nick = std::make_shared<lmdb::dbi>();

	// Initialize databases
	{
		auto wtxn = lmdb::txn::begin(*env);
		*cs_rptr = lmdb::dbi::open(wtxn, "cs_rptr_dbi", MDB_CREATE);
		*zone_ip4 = lmdb::dbi::open(wtxn, "zone_ip4", MDB_CREATE);
		*zone_ip6 = lmdb::dbi::open(wtxn, "zone_ip6", MDB_CREATE);
		*zone_nick = lmdb::dbi::open(wtxn, "zone_nick", MDB_CREATE);

		wtxn.commit();
	}

	ircddb::app* ircapp = new ircddb::app("u-KO6JXH", {{"ircv4.openquad.net", "", "#dstar", 9007, AF_INET}}, env, cs_rptr, zone_ip4, zone_ip6, zone_nick);

	dgate::app app("KO6JXH", {'C'}, std::unique_ptr<ircddb::app>(ircapp), env, cs_rptr, zone_ip4, zone_ip6, zone_nick);

	app.run();
}
