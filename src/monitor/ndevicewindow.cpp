/* 
 * Device window display.
 * Copyright (C) 2007 Petr Kubanek <petr@kubanek.net>
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

#include "nmonitor.h"
#include "ndevicewindow.h"

#include "../utils/timestamp.h"
#include "../utils/rts2displayvalue.h"
#include "../utils/riseset.h"

using namespace rts2ncurses;

NDeviceWindow::NDeviceWindow (Rts2Conn * in_connection):NSelWindow (10, 1, COLS - 10, LINES - 25)
{
	connection = in_connection;
	connection->resetInfoTime ();
	valueBox = NULL;
	valueBegins = 20;
	draw ();
}

NDeviceWindow::~NDeviceWindow ()
{
}

void NDeviceWindow::printState ()
{
	wattron (window, A_REVERSE);
	if (connection->getErrorState ())
		wcolor_set (window, CLR_FAILURE, NULL);
	mvwprintw (window, 0, 2, "%s %s (%x) %x", connection->getName (), connection->getStateString ().c_str (), connection->getState (), connection->getFullBopState ());
	wcolor_set (window, CLR_DEFAULT, NULL);
	wattroff (window, A_REVERSE);
}

void NDeviceWindow::printValue (const char *name, const char *value, bool writable)
{
	wprintw (getWriteWindow (), "%c %-20s %30s\n", ((writable) ? 'W' : ' '), name, value);
}

void NDeviceWindow::printValue (rts2core::Value * value)
{
	// customize value display
	std::ostringstream _os;
	switch (value->getFlags () & RTS2_VALUE_ERRORMASK)
	{
		case RTS2_VALUE_WARNING:
			wcolor_set (getWriteWindow (), CLR_WARNING, NULL);
			break;
		case RTS2_VALUE_ERROR:
			wcolor_set (getWriteWindow (), CLR_FAILURE, NULL);
			break;
		default:
			if (value->getWriteToFits ())
				wcolor_set (getWriteWindow (), CLR_FITS, NULL);
			else
				wcolor_set (getWriteWindow (), CLR_DEFAULT, NULL);
	}
	// ultra special handling of SCRIPT value
	if (value->getValueDisplayType () == RTS2_DT_SCRIPT)
	{
		wprintw (getWriteWindow (), "  %-20s ", value->getName ().c_str ());
		wcolor_set (getWriteWindow (), CLR_DEFAULT, NULL);
		const char *valStart = value->getValue ();
		if (!valStart)
			return;
		const char *valTop = valStart;
		int scriptPosition = connection->getValueInteger ("scriptPosition");
		int scriptEnd = connection->getValueInteger ("scriptLen") + scriptPosition;

		while (*valTop && (valTop - valStart < scriptPosition))
		{
			waddch (getWriteWindow (), *valTop);
			valTop++;
		}
		wcolor_set (getWriteWindow (), CLR_SCRIPT_CURRENT, NULL);
		while (*valTop && (valTop - valStart < scriptEnd))
		{
			waddch (getWriteWindow (), *valTop);
			valTop++;
		}
		wcolor_set (getWriteWindow (), CLR_DEFAULT, NULL);
		while (*valTop)
		{
			waddch (getWriteWindow (), *valTop);
			valTop++;
		}
		waddch (getWriteWindow (), '\n');
		return;
	}
	switch (value->getValueType ())
	{
		case RTS2_VALUE_RADEC:
			{
				if (value->getValueDisplayType () == RTS2_DT_DEGREES)
				{
					LibnovaDeg v_rd (((rts2core::ValueRaDec *) value)->getRa ());
					LibnovaDeg v_dd (((rts2core::ValueRaDec *) value)->getDec ());
					_os << v_rd << " " << v_dd;
				}
				else
				{
					LibnovaRaDec v_radec (((rts2core::ValueRaDec *) value)->getRa (), ((rts2core::ValueRaDec *) value)->getDec ());
					_os << v_radec;
				}
				printValue (value->getName ().c_str (), _os.str().c_str (), value->isWritable ());
			}
			break;
		case RTS2_VALUE_ALTAZ:
			{
				LibnovaHrz hrz (((rts2core::ValueAltAz *) value)->getAlt (), ((rts2core::ValueAltAz *) value)->getAz ());
				_os << hrz;
				printValue (value->getName ().c_str (), _os.str().c_str (), value->isWritable ());
			}
			break;
		case RTS2_VALUE_SELECTION:
			wprintw (getWriteWindow (), "%c %-20s %5i %24s\n", value->isWritable () ? 'W' : ' ', value->getName ().c_str (), value->getValueInteger (), ((rts2core::ValueSelection *) value)->getSelName ());
			break;
		default:
			printValue (value->getName ().c_str (), getDisplayValue (value).c_str (), value->isWritable ());
	}
}

void NDeviceWindow::drawValuesList ()
{
	gettimeofday (&tvNow, NULL);
	now = tvNow.tv_sec + tvNow.tv_usec / USEC_SEC;

	maxrow = 0;

	for (rts2core::ValueVector::iterator iter = connection->valueBegin (); iter != connection->valueEnd (); iter++)
	{
		maxrow++;
		printValue (*iter);
	}
}

rts2core::Value * NDeviceWindow::getSelValue ()
{
	int s = getSelRow ();
	if (s >= 0)
		return connection->valueAt (s);
	return NULL;
}

void NDeviceWindow::printValueDesc (rts2core::Value * val)
{
	wattron (window, A_REVERSE);
	mvwprintw (window, getHeight () - 1, 2, "D: \"%s\"",
		val->getDescription ().c_str ());
	wattroff (window, A_REVERSE);
}

void NDeviceWindow::endValueBox ()
{
	delete valueBox;
	valueBox = NULL;
}

void NDeviceWindow::createValueBox ()
{
	int s = getSelRow ();
	if (s < 0)
		return;
	rts2core::Value *val = connection->valueAt (s);
	if (!val || val->isWritable () == false)
		return;
	s -= getPadoffY ();
	switch (val->getValueType ())
	{
		case RTS2_VALUE_BOOL:
			valueBox = new ValueBoxBool (this, (rts2core::ValueBool *) val, 21, s - 1);
			break;
		case RTS2_VALUE_STRING:
			valueBox = new ValueBoxString (this, (rts2core::ValueString *) val, 21, s - 1);
			break;
		case RTS2_VALUE_INTEGER:
			valueBox = new ValueBoxInteger (this, (rts2core::ValueInteger *) val, 21, s);
			break;
		case RTS2_VALUE_LONGINT:
			valueBox = new ValueBoxLongInteger (this, (rts2core::ValueLong *) val, 21, s);
			break;
		case RTS2_VALUE_FLOAT:
			valueBox = new ValueBoxFloat (this, (rts2core::ValueFloat *) val, 21, s);
			break;
		case RTS2_VALUE_DOUBLE:
		case RTS2_VALUE_MMAX | RTS2_VALUE_DOUBLE:
			valueBox = new ValueBoxDouble (this, (rts2core::ValueDouble *) val, 21, s);
			break;
		case RTS2_VALUE_SELECTION:
			valueBox = new ValueBoxSelection (this, (rts2core::ValueSelection *) val, 21, s);
			break;
		case RTS2_VALUE_TIME:
			valueBox = new ValueBoxTimeDiff (this, (rts2core::ValueTime *) val, 21, s);
			break;
		case RTS2_VALUE_RADEC:
			valueBox = new ValueBoxPair (this, (rts2core::ValueRaDec *) val, 21, s, "RA", "DEC");
			break;
		case RTS2_VALUE_ALTAZ:
			valueBox = new ValueBoxPair (this, (rts2core::ValueRaDec *) val, 21, s, "ALT", "AZ");
			break;
		default:
			switch (val->getValueExtType ())
			{
				case RTS2_VALUE_RECTANGLE:
					valueBox = new ValueBoxRectangle (this, (rts2core::ValueRectangle *) val, 21, s - 1);
					break;
				case RTS2_VALUE_ARRAY:
					valueBox = new ValueBoxArray (this, (rts2core::ValueArray *) val, 21, s);
					break;
				default:
					logStream (MESSAGE_WARNING) << "cannot find box for value '" <<  val->getName () << " type " << val->getValueType () << sendLog;
					valueBox = new ValueBoxString (this, val, 21, s - 1);
					break;

			}
			break;
	}
}

keyRet NDeviceWindow::injectKey (int key)
{
	keyRet
		ret;
	switch (key)
	{
		case KEY_ENTER:
		case K_ENTER:
			// don't create new box if one already exists
			if (valueBox)
				break;
		case KEY_F (6):
			if (valueBox)
				endValueBox ();
			createValueBox ();
			return RKEY_HANDLED;
	}
	if (valueBox)
	{
		ret = valueBox->injectKey (key);
		if (ret == RKEY_ENTER || ret == RKEY_ESC)
		{
			if (ret == RKEY_ENTER)
				valueBox->sendValue (connection);
			endValueBox ();
			return RKEY_HANDLED;
		}
		return ret;
	}
	return NSelWindow::injectKey (key);
}

void NDeviceWindow::draw ()
{
	NSelWindow::draw ();
	werase (getWriteWindow ());
	drawValuesList ();

	wcolor_set (getWriteWindow (), CLR_DEFAULT, NULL);
	mvwvline (getWriteWindow (), 0, valueBegins, ACS_VLINE,	(maxrow > getHeight ()? maxrow + 1 : getHeight ()));
	mvwaddch (window, 0, valueBegins + 1, ACS_TTEE);
	mvwaddch (window, getHeight () - 1, valueBegins + 1, ACS_BTEE);

	printState ();
	rts2core::Value *val = getSelValue ();
	if (val != NULL)
		printValueDesc (val);
	winrefresh ();
}

void NDeviceWindow::winrefresh ()
{
	NSelWindow::winrefresh ();
	if (valueBox)
		valueBox->draw ();
}

bool NDeviceWindow::setCursor ()
{
	if (valueBox)
		return valueBox->setCursor ();
	return NSelWindow::setCursor ();
}

NDeviceCentralWindow::NDeviceCentralWindow (Rts2Conn * in_connection):NDeviceWindow (in_connection)
{
}

NDeviceCentralWindow::~NDeviceCentralWindow (void)
{
}

void NDeviceCentralWindow::printValues ()
{
	// print statusChanges

	rts2core::Value *nextState = getConnection ()->getValue ("next_state");
	if (nextState && nextState->getValueType () == RTS2_VALUE_SELECTION)
	{
		for (std::vector < FutureStateChange >::iterator iter = stateChanges.begin (); iter != stateChanges.end (); iter++)
		{
			std::ostringstream _os;
			_os << Timestamp ((*iter).getEndTime ()) << " (" << TimeDiff (now, (*iter).getEndTime ()) << ")";

			printValue (((rts2core::ValueSelection *) nextState)->getSelName ((*iter).getState ()), _os.str ().c_str (), false);
		}
	}
}

void NDeviceCentralWindow::drawValuesList ()
{
	NDeviceWindow::drawValuesList ();

	if (!getConnection ()->infoTimeChanged ())
	{
		printValues ();
		return;
	}

	rts2core::Value *valLng = getConnection ()->getValue ("longitude");
	rts2core::Value *valLat = getConnection ()->getValue ("latitude");

	if (valLng && valLat && !isnan (valLng->getValueDouble ()) && !isnan (valLat->getValueDouble ()))
	{
		struct ln_lnlat_posn observer;

		observer.lng = valLng->getValueDouble ();
		observer.lat = valLat->getValueDouble ();

		// get next night, or get beginnign of current night

		rts2core::Value *valNightHorizon = getConnection ()->getValue ("night_horizon");
		rts2core::Value *valDayHorizon = getConnection ()->getValue ("day_horizon");

		rts2core::Value *valEveningTime = getConnection ()->getValue ("evening_time");
		rts2core::Value *valMorningTime = getConnection ()->getValue ("morning_time");

		if (valNightHorizon
			&& valDayHorizon
			&& !isnan (valNightHorizon->getValueDouble ())
			&& !isnan (valDayHorizon->getValueDouble ()) && valEveningTime
			&& valMorningTime)
		{
			stateChanges.clear ();

			int curr_type = -1, next_type = -1;
			time_t t_start;
			time_t t_start_t;
			time_t ev_time = tvNow.tv_sec;

			while (ev_time < (tvNow.tv_sec + 86400))
			{
				t_start = ev_time;
				t_start_t = ev_time + 1;
				next_event (&observer, &t_start_t, &curr_type, &next_type,
					&ev_time, valNightHorizon->getValueDouble (),
					valDayHorizon->getValueDouble (),
					valEveningTime->getValueInteger (),
					valMorningTime->getValueInteger ());
				stateChanges.push_back (FutureStateChange (curr_type, t_start));
			}
		}

	}
	printValues ();
}
