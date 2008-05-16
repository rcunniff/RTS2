#include "rts2sehex.h"

Rts2Path::~Rts2Path (void)
{
	for (top = begin (); top != end (); top++)
	{
		delete *top;
	}
	clear ();
}


void
Rts2Path::addRaDec (double in_ra, double in_dec)
{
	struct ln_equ_posn *newPos = new struct ln_equ_posn;
	newPos->ra = in_ra;
	newPos->dec = in_dec;
	push_back (newPos);
}


double
Rts2SEHex::getRa ()
{
	return path.getRa () * ra_size;
}


double
Rts2SEHex::getDec ()
{
	return path.getDec () * dec_size;
}


bool Rts2SEHex::endLoop ()
{
	return !path.haveNext ();
}


bool
Rts2SEHex::getNextLoop ()
{
	if (path.getNext ())
	{
		changeEl->setChangeRaDec (getRa (), getDec ());
		return false;
	}
	afterBlockEnd ();
	return true;
}


void
Rts2SEHex::constructPath ()
{
	// construct path
	#define SQRT3 0.866
	path.addRaDec (-1, 0);
	path.addRaDec (0.5, SQRT3);
	path.addRaDec (1, 0);
	path.addRaDec (0.5, -SQRT3);
	path.addRaDec (-0.5, -SQRT3);
	path.addRaDec (-1, 0);
	path.addRaDec (0.5, SQRT3);
	#undef SQRT3
	path.endPath ();
}


void
Rts2SEHex::afterBlockEnd ()
{
	Rts2ScriptElementBlock::afterBlockEnd ();
	path.rewindPath ();
	bool en = true;
	script->getMaster ()->
		postEvent (new Rts2Event (EVENT_QUICK_ENABLE, (void *) &en));
}


Rts2SEHex::Rts2SEHex (Rts2Script * in_script, char new_device[DEVICE_NAME_SIZE], double in_ra_size, double in_dec_size):
Rts2ScriptElementBlock (in_script)
{
	deviceName = new char[strlen (new_device) + 1];
	strcpy (deviceName, new_device);
	ra_size = in_ra_size;
	dec_size = in_dec_size;
	changeEl = NULL;
}


Rts2SEHex::~Rts2SEHex (void)
{
	delete [] deviceName;
	changeEl = NULL;
}


void
Rts2SEHex::beforeExecuting ()
{
	if (!path.haveNext ())
		constructPath ();

	if (path.haveNext ())
	{
		changeEl = new Rts2ScriptElementChange (script, deviceName, getRa (), getDec ());
		addElement (changeEl);
	}

	bool en = false;
	if (script->getMaster ())
		script->getMaster ()->postEvent (new Rts2Event (EVENT_QUICK_ENABLE, (void *) &en));
}


void
Rts2SEFF::constructPath ()
{
	path.addRaDec (-2, -2);
	path.addRaDec (1, 0);
	path.addRaDec (1, 0);
	path.addRaDec (1, 0);
	path.addRaDec (1, 0);
	path.addRaDec (0, 1);
	path.addRaDec (-1, 0);
	path.addRaDec (-1, 0);
	path.addRaDec (-1, 0);
	path.addRaDec (-1, 0);
	path.addRaDec (0, 1);
	path.addRaDec (1, 0);
	path.addRaDec (1, 0);
	path.addRaDec (1, 0);
	path.addRaDec (1, 0);
	path.addRaDec (0, 1);
	path.addRaDec (-1, 0);
	path.addRaDec (-1, 0);
	path.addRaDec (-1, 0);
	path.addRaDec (-1, 0);
	path.addRaDec (0, 1);
	path.addRaDec (1, 0);
	path.addRaDec (1, 0);
	path.addRaDec (1, 0);
	path.addRaDec (1, 0);
	path.addRaDec (-2, -2);
	path.endPath ();
}


Rts2SEFF::Rts2SEFF (Rts2Script * in_script, char new_device[DEVICE_NAME_SIZE], double in_ra_size, double in_dec_size)
:Rts2SEHex (in_script, new_device, in_ra_size, in_dec_size)
{
}


Rts2SEFF::~Rts2SEFF (void)
{

}
