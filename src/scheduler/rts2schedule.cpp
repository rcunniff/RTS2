/*
 * One observing schedule.
 * Copyright (C) 2008 Petr Kubanek <petr@kubanek.net>
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

#include "rts2schedule.h"
#include "../utilsdb/accountset.h"
#include "../utilsdb/rts2targetset.h"

using namespace rts2db;

Rts2Schedule::Rts2Schedule (double _JDstart, double _JDend, struct ln_lnlat_posn *_obs):
std::vector <Rts2SchedObs*> ()
{
	JDstart = _JDstart;
	JDend = _JDend;
	observer = _obs;

	tarSet = NULL;

	visRatio = nan ("f");
	altMerit = nan ("f");
}


Rts2Schedule::Rts2Schedule (Rts2Schedule *sched1, Rts2Schedule *sched2, unsigned int crossPoint)
{
	// fill in parameters..
  	JDstart = sched1->JDstart;
	JDend = sched1->JDend;
	observer = sched1->observer;

	tarSet = sched1->tarSet;

	visRatio = nan ("f");
	altMerit = nan ("f");

	unsigned int i;
	Rts2SchedObs *parent;
	// fill in schedobs
	for (i = 0; i < crossPoint; i++)
	{
		parent = (*sched1)[i];
		push_back (new Rts2SchedObs (parent->getTarget (), parent->getJDStart (), parent->getLoopCount ()));
	}

	for (; i < sched2->size (); i++)
	{
		parent = (*sched2)[i];
		push_back (new Rts2SchedObs (parent->getTarget (), parent->getJDStart (), parent->getLoopCount ()));
	}
}


Rts2Schedule::~Rts2Schedule (void)
{
	for (Rts2Schedule::iterator iter = begin (); iter != end (); iter++)
		delete (*iter);
	clear ();
}


Target *
Rts2Schedule::randomTarget ()
{
	// random selection of observation
	return (*tarSet)[randomNumber (0, tarSet->size ())];
}


Rts2SchedObs *
Rts2Schedule::randomSchedObs (double JD)
{
	visRatio = nan ("f");
	altMerit = nan ("f");
	return new Rts2SchedObs (randomTarget (), JD, 1);
}


int
Rts2Schedule::constructSchedule (Rts2TargetSet *_tarSet)
{
	tarSet = _tarSet;
	double JD = JDstart;
	while (JD < JDend)
	{
		push_back (randomSchedObs (JD));
		JD += (double) 10.0 / 1440.0;
	}
	return 0;
}


double
Rts2Schedule::visibilityRatio ()
{
	if (!isnan (visRatio))
		return visRatio;

	unsigned int visible = 0;
	for (Rts2Schedule::iterator iter = begin (); iter != end (); iter++)
	{
		if ((*iter)->isVisible ())
			visible ++;
	}
	visRatio = (double) visible / size ();
	return visRatio;
}


double
Rts2Schedule::altitudeMerit ()
{
	if (!isnan (altMerit))
		return altMerit;

	altMerit = 0;
	for (Rts2Schedule::iterator iter = begin (); iter != end (); iter++)
	{
		altMerit += (*iter)->altitudeMerit (JDstart, JDend);
	}
	altMerit /= size ();
	return altMerit;
}


double
Rts2Schedule::accountMerit ()
{
	AccountSet *accountset = AccountSet::instance ();

	// map for storing calculated account time by account ids
	std::map <int, double> observedShares;

	// and fill map with all possible account IDs
	for (AccountSet::iterator iter = accountset->begin (); iter != accountset->end (); iter++)
	{
		observedShares[(*iter).first] = 0;
	}


	double sumDur = 0;

	for (Rts2Schedule::iterator iter = begin (); iter != end (); iter++)
	{
		observedShares[(*iter)->getAccountId ()] += (*iter)->getTotalDuration ();
		sumDur += (*iter)->getTotalDuration ();
	}

	// deviances and sum them
	double ret = 0;

	for (AccountSet::iterator iter = accountset->begin (); iter != accountset->end (); iter++)
	{
		double sh = (*iter).second->getShare () / accountset->getShareSum ();
		ret += fabs (observedShares[(*iter).first] / sumDur - sh) / sh;
	}

	return (double) 1.0 / ret;
}


std::ostream &
operator << (std::ostream & _os, Rts2Schedule & schedule)
{
	for (Rts2Schedule::iterator iter = schedule.begin (); iter != schedule.end (); iter++)
	{
		_os << *(*iter) << std::endl;
	}
	return _os;
}
