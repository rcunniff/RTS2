/* 
 * Receive and reacts to Auger showers.
 * Copyright (C) 2007-2009 Petr Kubanek <petr@kubanek.net>
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

#include "augershooter.h"
#include "../utils/libnova_cpp.h"
#include "../utils/rts2command.h"

using namespace rts2grbd;

#define OPT_TRIGERING     OPT_LOCAL + 707

DevAugerShooter::DevAugerShooter (int in_argc, char **in_argv):
Rts2DeviceDb (in_argc, in_argv, DEVICE_TYPE_AUGERSH, "AUGRSH")
{
	shootercnn = NULL;
	port = 1240;
	addOption ('s', "shooter_port", 1, "port on which to listen for auger connection");
	addOption (OPT_TRIGERING, "disable-triggering", 0, "only record triggers, do not pass them to executor");

	createValue (triggeringEnabled, "triggering_enabled", "if true, shooter will trigger executor", false);
	triggeringEnabled->setValueBool (true);

	createValue (minEnergy, "min_energy", "minimal shower energy", false);
	minEnergy->setValueDouble (10);

	createValue (maxTime, "max_time", "maximal time between shower and its observations", false);
	maxTime->setValueInteger (600);

	createValue (lastAugerDate, "last_date", "date of last shower", false);
	createValue (lastAugerRa, "last_ra", "RA of last shower", false, RTS2_DT_RA);
	createValue (lastAugerDec, "last_dec", "DEC of last shower", false, RTS2_DT_DEC);
}


DevAugerShooter::~DevAugerShooter (void)
{
}


int DevAugerShooter::processOption (int in_opt)
{
	switch (in_opt)
	{
		case 's':
			port = atoi (optarg);
			break;
		case OPT_TRIGERING:
			triggeringEnabled->setValueBool (false);
			break;
		default:
			return Rts2DeviceDb::processOption (in_opt);
	}
	return 0;
}

int DevAugerShooter::reloadConfig ()
{
	int ret = Rts2DeviceDb::reloadConfig ();
	if (ret)
		return ret;

	Rts2Config *config = Rts2Config::instance ();
	minEnergy->setValueDouble (config->getDoubleDefault ("augershooter", "minenergy", minEnergy->getValueDouble ()));

	maxTime->setValueInteger (config->getIntegerDefault ("augershooter", "maxtime", maxTime->getValueInteger ()));

	return 0;
}

int DevAugerShooter::setValue (Rts2Value *old_value, Rts2Value *new_value)
{
	if (old_value == minEnergy || old_value == maxTime || old_value == triggeringEnabled)
		return 0;
	return Rts2DeviceDb::setValue (old_value, new_value);
}


int DevAugerShooter::init ()
{
	int ret;
	ret = Rts2DeviceDb::init ();
	if (ret)
		return ret;

	shootercnn = new ConnShooter (port, this, minEnergy, maxTime);

	ret = shootercnn->init ();

	if (ret)
		return ret;

	addConnection (shootercnn);

	return ret;
}


int DevAugerShooter::newShower (double lastDate, double ra, double dec)
{
	if (triggeringEnabled->getValueBool () == false)
	{
		logStream (MESSAGE_WARNING) << "Triggering is disabled, trigger " << LibnovaDateDouble (lastDate) << "not executed." << sendLog;
	}
	Rts2Conn *exec;
	lastAugerDate->setValueDouble (lastDate);
	lastAugerRa->setValueDouble (ra);
	lastAugerDec->setValueDouble (dec);
	exec = getOpenConnection ("EXEC");
	if (exec)
	{
		exec->queCommand (new Rts2CommandExecShower (this));
	}
	else
	{
		logStream (MESSAGE_ERROR) << "FATAL! No executor running to post shower!" << sendLog;
		return -1;
	}
	return 0;
}


bool DevAugerShooter::wasSeen (double lastDate, double ra, double dec)
{
	return (fabs (lastDate - lastAugerDate->getValueDouble ()) < 5
		&& ra == lastAugerRa->getValueDouble ()
		&& dec == lastAugerDec->getValueDouble ());
}


int main (int argc, char **argv)
{
	DevAugerShooter device = DevAugerShooter (argc, argv);
	return device.run ();
}
