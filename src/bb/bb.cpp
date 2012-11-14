/**
 * RTS2 Big Brother server.
 * Copyright (C) 2010 Petr Kubanek <petr@kubanek.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "bb.h"

using namespace XmlRpc;
using namespace rts2bb;

BB::BB (int argc, char ** argv):
	rts2db::DeviceDb (argc, argv, 0, "BB"),
	bbApi ("/api", this, this)
{
	rpcPort = 8889;

	createValue (debugConn, "debug_conn", "debug connections calls", false, RTS2_VALUE_WRITABLE | RTS2_DT_ONOFF);
	debugConn->setValueBool (false);

	addOption ('p', NULL, 1, "RPC listening port");
}

int BB::processOption (int opt)
{
	switch (opt)
	{
		case 'p':
			rpcPort = atoi (optarg);
			break;
		default:
			return rts2db::DeviceDb::processOption (opt);
	}
	return 0;
}

int BB::init ()
{
	int ret;

	ret = rts2db::DeviceDb::init ();
	if (ret)
		return ret;

	if (printDebug ())
		XmlRpc::setVerbosity (5);

	XmlRpcServer::bindAndListen (rpcPort);
	XmlRpcServer::enableIntrospection (true);

	return ret;
}

void BB::addSelectSocks (fd_set &read_set, fd_set &write_set, fd_set &exp_set)
{
	rts2db::DeviceDb::addSelectSocks (read_set, write_set, exp_set);
	XmlRpcServer::addToFd (&read_set, &write_set, &exp_set);
}

void BB::selectSuccess (fd_set &read_set, fd_set &write_set, fd_set &exp_set)
{
	rts2db::DeviceDb::selectSuccess (read_set, write_set, exp_set);
	XmlRpcServer::checkFd (&read_set, &write_set, &exp_set);
}

int main (int argc, char **argv)
{
	BB device (argc, argv);
	return device.run ();
}