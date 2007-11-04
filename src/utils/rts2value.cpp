/* 
 * Various value classes.
 * Copyright (C) 2003-2007 Petr Kubanek <petr@kubanek.net>
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

#include <math.h>
#include <stdio.h>
#include <sstream>

#include "rts2block.h"
#include "rts2value.h"

#include <libnova/libnova.h>

Rts2Value::Rts2Value (std::string in_val_name)
{
	valueName = in_val_name;
	rts2Type = 0;
}


Rts2Value::Rts2Value (std::string in_val_name, std::string in_description,
bool writeToFits, int32_t flags)
{
	valueName = in_val_name;
	rts2Type = 0;
	description = in_description;
	if (writeToFits)
		setWriteToFits ();
	setValueFlags (flags);
}


int
Rts2Value::doOpValue (char op, Rts2Value * old_value)
{
	switch (op)
	{
		case '=':
			return 0;
		default:
			logStream (MESSAGE_ERROR) << "unknow op '" << op << "' for type " <<
				getValueType () << sendLog;
			return -1;
	}
}


int
Rts2Value::sendTypeMetaInfo (Rts2Conn * connection)
{
	std::ostringstream _os;
	_os
		<< PROTO_METAINFO << " "
		<< rts2Type << " "
		<< '"' << getName () << "\" " << '"' << description << "\" ";
	return connection->sendMsg (_os.str ());
}


int
Rts2Value::sendMetaInfo (Rts2Conn * connection)
{
	int ret;
	ret = sendTypeMetaInfo (connection);
	if (ret < 0)
		return ret;

	return send (connection);
}


int
Rts2Value::send (Rts2Conn * connection)
{
	return connection->sendValueRaw (getName (), getValue ());
}


Rts2ValueString::Rts2ValueString (std::string in_val_name):
Rts2Value (in_val_name)
{
	value = NULL;
	rts2Type |= RTS2_VALUE_STRING;
}


Rts2ValueString::Rts2ValueString (std::string in_val_name, std::string in_description, bool writeToFits, int32_t flags):
Rts2Value (in_val_name, in_description, writeToFits, flags)
{
	value = NULL;
	rts2Type |= RTS2_VALUE_STRING;
}


const char *
Rts2ValueString::getValue ()
{
	return value;
}


int
Rts2ValueString::setValue (Rts2Conn * connection)
{
	char *new_value;
	if (connection->paramNextString (&new_value) || !connection->paramEnd ())
		return -2;
	setValueString (new_value);
	return 0;
}


int
Rts2ValueString::setValueString (const char *in_value)
{
	delete[]value;
	if (!in_value)
	{
		value = NULL;
		return -1;
	}
	value = new char[strlen (in_value) + 1];
	strcpy (value, in_value);
	return 0;
}


int
Rts2ValueString::setValueInteger (int in_value)
{
	delete[]value;
	asprintf (&value, "%i", in_value);
	return 0;
}


int
Rts2ValueString::send (Rts2Conn * connection)
{
	return connection->sendValue (getName (), getValue ());
}


void
Rts2ValueString::setFromValue (Rts2Value * newValue)
{
	setValueString (newValue->getValue ());
}


Rts2ValueInteger::Rts2ValueInteger (std::string in_val_name):
Rts2Value (in_val_name)
{
	value = 0;
	rts2Type |= RTS2_VALUE_INTEGER;
}


Rts2ValueInteger::Rts2ValueInteger (std::string in_val_name, std::string in_description, bool writeToFits, int32_t flags):
Rts2Value (in_val_name, in_description, writeToFits, flags)
{
	value = 0;
	rts2Type |= RTS2_VALUE_INTEGER;
}


const char *
Rts2ValueInteger::getValue ()
{
	sprintf (buf, "%i", value);
	return buf;
}


int
Rts2ValueInteger::setValue (Rts2Conn * connection)
{
	int new_value;
	if (connection->paramNextInteger (&new_value) || !connection->paramEnd ())
		return -2;
	value = new_value;
	return 0;
}


int
Rts2ValueInteger::setValueString (const char *in_value)
{
	return setValueInteger (atoi (in_value));
}


int
Rts2ValueInteger::doOpValue (char op, Rts2Value * old_value)
{
	switch (op)
	{
		case '+':
			return setValueInteger (old_value->getValueInteger () +
				getValueInteger ());
		case '-':
			return setValueInteger (old_value->getValueInteger () -
				getValueInteger ());
		case '=':
			return setValueInteger (getValueInteger ());
	}
	return Rts2Value::doOpValue (op, old_value);
}


void
Rts2ValueInteger::setFromValue (Rts2Value * newValue)
{
	setValueInteger (newValue->getValueInteger ());
}


Rts2ValueDouble::Rts2ValueDouble (std::string in_val_name):Rts2Value
(in_val_name)
{
	value = nan ("f");
	rts2Type |= RTS2_VALUE_DOUBLE;
}


Rts2ValueDouble::Rts2ValueDouble (std::string in_val_name, std::string in_description, bool writeToFits, int32_t flags):
Rts2Value (in_val_name, in_description, writeToFits, flags)
{
	value = nan ("f");
	rts2Type |= RTS2_VALUE_DOUBLE;
}


const char *
Rts2ValueDouble::getValue ()
{
	sprintf (buf, "%.20le", value);
	return buf;
}


const char *
Rts2ValueDouble::getDisplayValue ()
{
	double absv = fabs (value);
	if ((absv > 10e-3 && absv < 10e+8) || absv == 0)
		sprintf (buf, "%lf", value);
	else
		sprintf (buf, "%.20le", value);
	return buf;
}


int
Rts2ValueDouble::setValue (Rts2Conn * connection)
{
	double new_value;
	if (connection->paramNextDouble (&new_value) || !connection->paramEnd ())
		return -2;
	value = new_value;
	return 0;
}


int
Rts2ValueDouble::setValueString (const char *in_value)
{
	setValueDouble (atof (in_value));
	return 0;
}


int
Rts2ValueDouble::setValueInteger (int in_value)
{
	setValueDouble (in_value);
	return 0;
}


int
Rts2ValueDouble::doOpValue (char op, Rts2Value * old_value)
{
	switch (op)
	{
		case '+':
			setValueDouble (old_value->getValueDouble () + getValueDouble ());
			break;
		case '-':
			setValueDouble (old_value->getValueDouble () - getValueDouble ());
			break;
		default:
			return Rts2Value::doOpValue (op, old_value);
	}
	return 0;
}


void
Rts2ValueDouble::setFromValue (Rts2Value * newValue)
{
	setValueDouble (newValue->getValueDouble ());
}


Rts2ValueTime::Rts2ValueTime (std::string in_val_name):Rts2ValueDouble
(in_val_name)
{
	rts2Type = (~RTS2_VALUE_MASK & rts2Type) | RTS2_VALUE_TIME;
}


Rts2ValueTime::Rts2ValueTime (std::string in_val_name, std::string in_description, bool writeToFits, int32_t flags):
Rts2ValueDouble (in_val_name, in_description, writeToFits, flags)
{
	rts2Type = (~RTS2_VALUE_MASK & rts2Type) | RTS2_VALUE_TIME;
}


const char *
Rts2ValueTime::getDisplayValue ()
{
	sprintf (buf, "%lf", getValueDouble ());
	return buf;
}


Rts2ValueFloat::Rts2ValueFloat (std::string in_val_name):
Rts2Value (in_val_name)
{
	value = nan ("f");
	rts2Type |= RTS2_VALUE_FLOAT;
}


Rts2ValueFloat::Rts2ValueFloat (std::string in_val_name, std::string in_description, bool writeToFits, int32_t flags):
Rts2Value (in_val_name, in_description, writeToFits, flags)
{
	value = nan ("f");
	rts2Type |= RTS2_VALUE_FLOAT;
}


const char *
Rts2ValueFloat::getValue ()
{
	sprintf (buf, "%.20e", value);
	return buf;
}


const char *
Rts2ValueFloat::getDisplayValue ()
{
	double absv = fabs (value);
	if ((absv > 10e-3 && absv < 10e+5) || absv == 0)
		sprintf (buf, "%lf", value);
	else
		sprintf (buf, "%.20le", value);
	return buf;
}


int
Rts2ValueFloat::setValue (Rts2Conn * connection)
{
	float new_value;
	if (connection->paramNextFloat (&new_value) || !connection->paramEnd ())
		return -2;
	value = new_value;
	return 0;
}


int
Rts2ValueFloat::setValueString (const char *in_value)
{
	setValueDouble (atof (in_value));
	return 0;
}


int
Rts2ValueFloat::setValueInteger (int in_value)
{
	setValueDouble (in_value);
	return 0;
}


int
Rts2ValueFloat::doOpValue (char op, Rts2Value * old_value)
{
	switch (op)
	{
		case '+':
			setValueFloat (old_value->getValueFloat () + getValueFloat ());
			break;
		case '-':
			setValueFloat (old_value->getValueFloat () - getValueFloat ());
			break;
		default:
			return Rts2Value::doOpValue (op, old_value);
	}
	return 0;
}


void
Rts2ValueFloat::setFromValue (Rts2Value * newValue)
{
	setValueFloat (newValue->getValueFloat ());
}


Rts2ValueBool::Rts2ValueBool (std::string in_val_name):Rts2ValueInteger
(in_val_name)
{
	rts2Type = (~RTS2_VALUE_MASK & rts2Type) | RTS2_VALUE_BOOL;
}


Rts2ValueBool::Rts2ValueBool (std::string in_val_name, std::string in_description, bool writeToFits, int32_t flags):Rts2ValueInteger (in_val_name, in_description,
writeToFits,
flags)
{
	rts2Type = (~RTS2_VALUE_MASK & rts2Type) | RTS2_VALUE_BOOL;
}


int
Rts2ValueBool::setValueString (const char *in_value)
{
	if (!strcasecmp (in_value, "ON") || !strcasecmp (in_value, "TRUE")
		|| !strcasecmp (in_value, "YES") || !strcmp (in_value, "1"))
		setValueBool (true);
	else if (!strcasecmp (in_value, "OFF") || !strcasecmp (in_value, "FALSE")
		|| !strcasecmp (in_value, "NO") || !strcmp (in_value, "0"))
		setValueBool (false);
	else
		return -1;
	return 0;
}


const char *
Rts2ValueBool::getDisplayValue ()
{
	return getValueBool ()? "true" : "false";
}


Rts2ValueSelection::Rts2ValueSelection (std::string in_val_name):Rts2ValueInteger
(in_val_name)
{
	rts2Type = (~RTS2_VALUE_MASK & rts2Type) | RTS2_VALUE_SELECTION;
}


Rts2ValueSelection::Rts2ValueSelection (std::string in_val_name, std::string in_description, bool writeToFits, int32_t flags):Rts2ValueInteger (in_val_name, in_description,
writeToFits,
flags)
{
	rts2Type = (~RTS2_VALUE_MASK & rts2Type) | RTS2_VALUE_SELECTION;
}


Rts2ValueSelection::~Rts2ValueSelection ()
{
	values.clear ();
}


void
Rts2ValueSelection::deleteValues ()
{
	for (std::vector <SelVal>::iterator iter=selBegin (); iter!= selEnd (); iter++)
	{
		delete (*iter).data;
	}
	values.clear ();
}


int
Rts2ValueSelection::setValue (Rts2Conn * connection)
{
	char *new_value;
	if (connection->paramNextString (&new_value) || !connection->paramEnd ())
		return -2;
	// try if it's number
	int ret;
	ret = setValueString (new_value);
	if (ret)
		return -2;
	return 0;
}


int
Rts2ValueSelection::setValueString (const char *in_value)
{
	char *end;
	int ret = strtol (in_value, &end, 10);
	if (!*end)
	{
		return setValueInteger (ret);
	}
	ret = getSelIndex (std::string (in_value));
	if (ret < 0)
		return -1;
	return setValueInteger (ret);
}


int
Rts2ValueSelection::getSelIndex (std::string in_val)
{
	int i = 0;
	for (std::vector < SelVal >::iterator iter = selBegin (); iter != selEnd (); iter++, i++)
	{
		if ((*iter).name == in_val)
			return i;
	}
	return -2;
}


void
Rts2ValueSelection::copySel (Rts2ValueSelection * sel)
{
	for (std::vector < SelVal >::iterator iter = sel->selBegin (); iter != sel->selEnd (); iter++)
	{
		addSelVal (*iter);
	}
}


void
Rts2ValueSelection::addSelVals (const char **vals)
{
	while (*vals != NULL)
	{
		addSelVal (*vals);
		vals++;
	}
}


int
Rts2ValueSelection::sendTypeMetaInfo (Rts2Conn * connection)
{
	int ret;
	ret = Rts2ValueInteger::sendTypeMetaInfo (connection);
	if (ret)
		return ret;
	// now send selection values..
	std::ostringstream _os;

	for (std::vector < SelVal >::iterator iter = selBegin (); iter != selEnd (); iter++)
	{
		std::string val = (*iter).name;
		_os << PROTO_SELMETAINFO << " \"" << getName () << "\" \"" << val <<
			"\"\n";
	}

	return connection->sendMsg (_os.str ());
}


void
Rts2ValueSelection::duplicateSelVals (Rts2ValueSelection * otherValue)
{
	deleteValues ();
	for (std::vector < SelVal >::iterator iter = otherValue->selBegin (); iter != otherValue->selEnd (); iter++)
	{
		addSelVal (*iter);
	}
}


Rts2ValueLong::Rts2ValueLong (std::string in_val_name):
Rts2Value (in_val_name)
{
	value = 0;
	rts2Type |= RTS2_VALUE_LONGINT;
}


Rts2ValueLong::Rts2ValueLong (std::string in_val_name, std::string in_description, bool writeToFits, int32_t flags):
Rts2Value (in_val_name, in_description, writeToFits, flags)
{
	value = 0;
	rts2Type |= RTS2_VALUE_LONGINT;
}


const char *
Rts2ValueLong::getValue ()
{
	sprintf (buf, "%li", value);
	return buf;
}


int
Rts2ValueLong::setValue (Rts2Conn * connection)
{
	long int new_value;
	if (connection->paramNextLong (&new_value) || !connection->paramEnd ())
		return -2;
	value = new_value;
	return 0;
}


int
Rts2ValueLong::setValueString (const char *in_value)
{
	return setValueLong (atol (in_value));
}


int
Rts2ValueLong::setValueInteger (int in_value)
{
	return setValueLong (in_value);
}


int
Rts2ValueLong::doOpValue (char op, Rts2Value * old_value)
{
	switch (op)
	{
		case '+':
			return setValueLong (old_value->getValueLong () + getValueLong ());
		case '-':
			return setValueLong (old_value->getValueLong () - getValueLong ());
		case '=':
			return setValueLong (getValueLong ());
	}
	return Rts2Value::doOpValue (op, old_value);
}


void
Rts2ValueLong::setFromValue (Rts2Value * newValue)
{
	setValueLong (newValue->getValueLong ());
}


Rts2Value *newValue (int rts2Type, std::string name, std::string desc)
{
	switch (rts2Type & RTS2_BASE_TYPE)
	{
		case RTS2_VALUE_STRING:
			return new Rts2ValueString (name, desc, rts2Type & RTS2_VALUE_FITS, rts2Type);
		case RTS2_VALUE_INTEGER:
			return new Rts2ValueInteger (name, desc, rts2Type & RTS2_VALUE_FITS, rts2Type);
		case RTS2_VALUE_TIME:
			return new  Rts2ValueTime (name, desc, rts2Type & RTS2_VALUE_FITS, rts2Type);
		case RTS2_VALUE_DOUBLE:
			return new Rts2ValueDouble (name, desc, rts2Type & RTS2_VALUE_FITS, rts2Type);
		case RTS2_VALUE_FLOAT:
			return new Rts2ValueFloat (name, desc, rts2Type & RTS2_VALUE_FITS, rts2Type);
		case RTS2_VALUE_BOOL:
			return new Rts2ValueBool (name, desc, rts2Type & RTS2_VALUE_FITS, rts2Type);
		case RTS2_VALUE_SELECTION:
			return new Rts2ValueSelection (name, desc, rts2Type & RTS2_VALUE_FITS, rts2Type);
		case RTS2_VALUE_LONGINT:
			return new Rts2ValueLong (name, desc, rts2Type & RTS2_VALUE_FITS, rts2Type);
	}
	logStream (MESSAGE_ERROR) << "unknow value name: " << name << " type: " << rts2Type << sendLog;
	return NULL;
}
