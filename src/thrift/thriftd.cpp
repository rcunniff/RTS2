/*
 * Thrift daemon.
 * Copyright (C) 2016-2017 Petr Kubanek <petr@kubanek.net>
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

// based on autogenerated Thrift skeleton

#include <pthread.h>
#include "device.h"
#include <libnova/libnova.h>

#include "ObservatoryService.h"
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TSimpleServer.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TBufferTransports.h>

using namespace ::apache::thrift;
using namespace ::apache::thrift::protocol;
using namespace ::apache::thrift::transport;
using namespace ::apache::thrift::server;

using boost::shared_ptr;

using namespace  ::rts2;

// RTS2 thrift service..
class ThriftD: public rts2core::Device
{
	public:
		ThriftD (int argc, char **argv);
		virtual int idle ();

		MountInfo mountInfo;
		DerotatorInfo derotatorInfo;

	protected:
		virtual int init ();
		virtual int willConnect (rts2core::NetworkAddress * _addr);

	private:
		pthread_t thrift_thr;
};

ThriftD *rts2Device;

class ObservatoryServiceHandler : virtual public ObservatoryServiceIf {
	public:
		ObservatoryServiceHandler() {
		}

		void infoMount(MountInfo& _return) {
			_return = rts2Device->mountInfo;
		}

		int32_t Slew(const RaDec& target) {
			rts2core::CommandMove cmd (rts2Device, NULL, target.ra, target.dec);
			rts2Device->queueCommandForType (DEVICE_TYPE_MOUNT, cmd);
			return 0;
		}

		int32_t Park() {
			rts2core::Command cmd (rts2Device, COMMAND_TELD_PARK);
			rts2Device->queueCommandForType (DEVICE_TYPE_MOUNT, cmd);
			return 0;
		}

		void infoDerotator(DerotatorInfo& _return) {
			_return = rts2Device->derotatorInfo;
		}
};

ThriftD::ThriftD (int argc, char **argv): rts2core::Device (argc, argv, DEVICE_TYPE_THRIFT, "THRIFT")
{
}

void *thrift_thread (void *args)
{
	int port = 9093;
	shared_ptr<ObservatoryServiceHandler> handler(new ObservatoryServiceHandler());
	shared_ptr<TProcessor> processor(new ObservatoryServiceProcessor(handler));
	shared_ptr<TServerTransport> serverTransport(new TServerSocket(port));
	shared_ptr<TTransportFactory> transportFactory(new TBufferedTransportFactory());
	shared_ptr<TProtocolFactory> protocolFactory(new TBinaryProtocolFactory());

	TSimpleServer server(processor, serverTransport, transportFactory, protocolFactory);
	server.serve();
	return NULL;
}

int ThriftD::init ()
{
	int ret = rts2core::Device::init ();
	if (ret)
		return ret;

	ret = rts2core::Device::doDaemonize ();
	if (ret)
		return ret;

	pthread_create (&thrift_thr, NULL, &thrift_thread, NULL);
	return 0;
}

int ThriftD::idle ()
{
	rts2core::Connection *telConn = getOpenConnection (DEVICE_TYPE_MOUNT);
	if (telConn != NULL)
	{
		rts2core::Value *val;
		rts2core::ValueRaDec *raDec;
		rts2core::ValueAltAz *altAz;

		val = telConn->getValue ("infotime");
		if (val != NULL)
		{
			mountInfo.infotime = val->getValueDouble ();
		}
		raDec = (rts2core::ValueRaDec *) telConn->getValue ("ORI");
		if (raDec != NULL)
		{
			mountInfo.ORI.ra = raDec->getRa ();
			mountInfo.ORI.dec = raDec->getDec ();
		}
		raDec = (rts2core::ValueRaDec *) telConn->getValue ("OFFS");
		if (raDec != NULL)
		{
			mountInfo.offsets.ra = raDec->getRa ();
			mountInfo.offsets.dec = raDec->getDec ();
		}
		raDec = (rts2core::ValueRaDec *) telConn->getValue ("TEL");
		if (raDec != NULL)
		{
			mountInfo.TEL.ra = raDec->getRa ();
			mountInfo.TEL.dec = raDec->getDec ();
		}
		altAz = (rts2core::ValueAltAz *) telConn->getValue ("TEL_");
		if (altAz != NULL)
		{
			mountInfo.HRZ.alt = altAz->getAlt ();
			mountInfo.HRZ.az = ln_range_degrees (altAz->getAz () + 180.0);
		}
		val = telConn->getValue ("JD");
		if (val != NULL)
		{
			mountInfo.JulianDay = val->getValueDouble ();
		}


	}
	return Device::idle ();
}

int ThriftD::willConnect (rts2core::NetworkAddress *_addr)
{
	if (_addr->getType () < getDeviceType () || (_addr->getType () == getDeviceType () && strcmp (_addr->getName (), getDeviceName ()) < 0))
		return 1;
	return 0;
}

int main (int argc, char **argv)
{
	rts2Device = new ThriftD (argc, argv);
	return rts2Device->run ();
}
