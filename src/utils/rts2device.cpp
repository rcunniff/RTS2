#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <getopt.h>

#include "status.h"
#include "rts2device.h"
#include "rts2command.h"

#define MINDATAPORT		5556
#define MAXDATAPORT		5656

Rts2Dev2DevConn::Rts2Dev2DevConn (Rts2Block * in_master, char *in_name):
Rts2Conn (in_master)
{
  setName (in_name);
  conn_state = CONN_RESOLVING_DEVICE;
  address = NULL;
}

Rts2Dev2DevConn::Rts2Dev2DevConn (Rts2Block * in_master, Rts2Address * in_addr):Rts2Conn
  (in_master)
{
  conn_state = CONN_CONNECTING;
  setName (in_addr->getName ());
  setAddress (in_addr);
  init ();
}

Rts2Dev2DevConn::~Rts2Dev2DevConn (void)
{
}

int
Rts2Dev2DevConn::init ()
{
  int ret;
  struct addrinfo *device_addr;
  if (!address)
    return -1;

  ret = address->getSockaddr (&device_addr);

  if (ret)
    return ret;
  sock =
    socket (device_addr->ai_family, device_addr->ai_socktype,
	    device_addr->ai_protocol);
  if (sock == -1)
    {
      return -1;
    }
  ret = fcntl (sock, F_SETFL, O_NONBLOCK);
  if (ret == -1)
    {
      return -1;
    }
  ret = connect (sock, device_addr->ai_addr, device_addr->ai_addrlen);
  freeaddrinfo (device_addr);
  if (ret == -1)
    {
      if (errno = EINPROGRESS)
	{
	  conn_state = CONN_CONNECTING;
	  return 0;
	}
      return -1;
    }
  connAuth ();
  return 0;
}

int
Rts2Dev2DevConn::idle ()
{
  switch (conn_state)
    {
    case CONN_CONNECTING:
      int err;
      int ret;
      socklen_t len = sizeof (err);

      ret = getsockopt (sock, SOL_SOCKET, SO_ERROR, &err, &len);
      if (ret)
	{
	  syslog (LOG_ERR, "Rts2Dev2DevConn::idle getsockopt %m");
	  conn_state = CONN_DELETE;
	  break;
	}
      if (err)
	{
	  syslog (LOG_ERR, "Rts2Dev2DevConn::idle getsockopt %s",
		  strerror (err));
	  conn_state = CONN_DELETE;
	  break;
	}
      connAuth ();
      break;
    }
  return Rts2Conn::idle ();
}

void
Rts2Dev2DevConn::setAddress (Rts2Address * in_addr)
{
  address = in_addr;
  setOtherType (address->getType ());
}

void
Rts2Dev2DevConn::addressAdded (Rts2Address * in_addr)
{
  if (isName (in_addr->getName ()))
    {
      setAddress (in_addr);
      init ();
    }
}

void
Rts2Dev2DevConn::connAuth ()
{
  master->getCentraldConn ()->
    queCommand (new Rts2CommandAuthorize (master, getName ()));
  conn_state = CONN_AUTH_PENDING;
}

void
Rts2Dev2DevConn::setKey (int in_key)
{
  Rts2Conn::setKey (in_key);
  if (conn_state == CONN_AUTH_PENDING)
    {
      // que to begining, send command
      // kill all runinng commands
      queSend (new Rts2CommandSendKey (master, in_key));
    }
}

int
Rts2DevConn::connectionError ()
{
  if (conn_state == CONN_AUTH_PENDING)
    master->authorize (NULL);	// cancel pendig authorization
  return -1;
}

int
Rts2DevConn::commandAuthorized ()
{
  if (isCommand ("ready"))
    {
      return master->ready (this);
    }
  else if (isCommand ("info"))
    {
      return master->info (this);
    }
  else if (isCommand ("base_info"))
    {
      return master->baseInfo (this);
    }
  sendCommandEnd (DEVDEM_E_SYSTEM, "devcon unknow command");
  return -1;
}

int
Rts2DevConn::command ()
{
  int ret;
  if (getCentraldId () != -1)	// authorized and running
    {
      ret = commandAuthorized ();
      if (ret == DEVDEM_E_HW)
	{
	  sendCommandEnd (DEVDEM_E_HW, "device error");
	  return -1;
	}
      return ret;
    }
  if (isCommand ("auth"))
    {
      int auth_id;
      int auth_key;
      if (paramNextInteger (&auth_id)
	  || paramNextInteger (&auth_key) || !paramEnd ())
	return -2;
      setCentraldId (auth_id);
      setKey (auth_key);
      ret = master->authorize (this);
      if (ret)
	{
	  sendCommandEnd (DEVDEM_E_SYSTEM,
			  "cannot authorize; try again later");
	  return -1;
	}
      conn_state = CONN_AUTH_PENDING;
      return -1;
    }
  return Rts2Conn::command ();
}

Rts2DevConn::Rts2DevConn (int in_sock, Rts2Device * in_master):Rts2Conn (in_sock,
	  in_master)
{
  master = in_master;
}

int
Rts2DevConn::add (fd_set * set)
{
  if (conn_state == CONN_AUTH_PENDING)
    return 0;
  return Rts2Conn::add (set);
}

int
Rts2DevConn::authorizationOK ()
{
  conn_state = CONN_AUTH_OK;
  master->sendStatusInfo (this);
  sendCommandEnd (0, "OK authorized");
  return 0;
}

int
Rts2DevConn::authorizationFailed ()
{
  setCentraldId (-1);
  conn_state = CONN_AUTH_FAILED;
  sendCommandEnd (DEVDEM_E_SYSTEM, "authorization failed");
  return 0;
}

void
Rts2DevConn::setHavePriority (int in_have_priority)
{
  if (havePriority () != in_have_priority)
    {
      Rts2Conn::setHavePriority (in_have_priority);
      if (havePriority ())
	{
	  send ("S priority 1 priority received");
	}
      else
	{
	  send ("S priority 0 priority lost");
	}
    }
}

Rts2DevConnMaster::Rts2DevConnMaster (Rts2Block * in_master,
				      char *in_device_host,
				      int in_device_port,
				      char *in_device_name,
				      int in_device_type,
				      char *in_master_host,
				      int in_master_port):
Rts2Conn (-1, in_master)
{
  device_host = in_device_host;
  device_port = in_device_port;
  strncpy (device_name, in_device_name, DEVICE_NAME_SIZE);
  device_type = in_device_type;
  strncpy (master_host, in_master_host, HOST_NAME_MAX);
  master_port = in_master_port;
  auth_conn = NULL;
}

int
Rts2DevConnMaster::registerDevice ()
{
  char *msg;
  int ret;
  if (!device_host)
    {
      device_host = new char[HOST_NAME_MAX];
      ret = gethostname (device_host, HOST_NAME_MAX);
    }
  if (ret < 0)
    return -1;
  asprintf (&msg, "register %s %i %s %i", device_name, device_type,
	    device_host, device_port);
  ret = send (msg);
  free (msg);
  return ret;
}

int
Rts2DevConnMaster::init ()
{
  struct hostent *master_info;
  struct sockaddr_in address;
  int ret;
  // init sock address
  address.sin_family = AF_INET;
  address.sin_port = htons (master_port);
  // get remote host
  if ((master_info = gethostbyname (master_host)))
    {
      address.sin_addr = *(struct in_addr *) master_info->h_addr;
    }
  else
    {
      return -1;
    }
  // get hostname
  if (ret < 0)
    return -1;
  sock = socket (PF_INET, SOCK_STREAM, 0);
  if (sock < 0)
    return -1;
  ret = connect (sock, (struct sockaddr *) &address, sizeof (address));
  if (ret < 0)
    {
      close (sock);
      sock = -1;
      return -1;
    }
  // have to wait for reply
  return registerDevice ();
}

int
Rts2DevConnMaster::command ()
{
  if (isCommand ("authorization_ok"))
    {
      if (auth_conn)
	{
	  int auth_id;
	  if (paramNextInteger (&auth_id))
	    return -1;
	  if (auth_conn->getCentraldId () != auth_id)
	    return -1;
	  auth_conn->authorizationOK ();
	  auth_conn = NULL;
	  return -1;
	}
      return 0;
    }
  else if (isCommand ("authorization_failed"))
    {
      if (auth_conn)
	{
	  int auth_id;
	  if (paramNextInteger (&auth_id))
	    return -1;
	  if (auth_conn->getCentraldId () != auth_id)
	    return -1;
	  auth_conn->authorizationFailed ();
	  auth_conn = NULL;
	  return -1;
	}
      return 0;
    }
  return Rts2Conn::command ();
}

int
Rts2DevConnMaster::message ()
{
  char *msg;
  if (paramNextString (&msg))
    return -1;
  if (!strcmp (msg, "priority_change"))
    {
      // change priority
      int priority_client;
      int timeout;
      if (paramNextInteger (&priority_client))
	return -1;
      if (paramNextInteger (&timeout))
	return -1;
      master->setPriorityClient (priority_client, timeout);
      return -1;
    }
  return -2;
}

int
Rts2DevConnMaster::informations ()
{
  char *name;
  int status_num;
  char *state_name;
  int state_value;
  if (paramNextString (&name) || paramNextInteger (&status_num)
      || paramNextString (&state_name) || paramNextInteger (&state_value)
      || !paramEnd ())
    return 0;
  return master->setMasterState (state_value);
}

int
Rts2DevConnMaster::status ()
{
  char *msg;
  int new_state;
  if (paramNextString (&msg) || paramNextInteger (&new_state) || !paramEnd ())
    return -1;
  return master->setMasterState (new_state);
}

int
Rts2DevConnMaster::authorize (Rts2DevConn * conn)
{
  char *msg;
  int ret;
  if (auth_conn)
    {
      if (!conn)
	auth_conn = NULL;	// request for canceling authorization
      return -1;		// authorization already pending, cannot authorize second device at same time
    }
  auth_conn = conn;
  asprintf (&msg, "authorize %i %i", auth_conn->getCentraldId (),
	    auth_conn->getKey ());
  ret = send (msg);
  free (msg);
  return ret;
}

int
Rts2DevConnData::init ()
{
  // find empty port
  sock = socket (PF_INET, SOCK_STREAM, 0);
  if (sock < 0)
    return -1;

  struct sockaddr_in server;
  int test_port;

  server.sin_family = AF_INET;
  server.sin_addr.s_addr = htonl (INADDR_ANY);

  // find empty port
  for (test_port = MINDATAPORT; test_port < MAXDATAPORT; test_port++)
    {
      server.sin_port = htons (test_port);
      if (bind (sock, (struct sockaddr *) &server, sizeof (server)) == 0)
	break;
    }
  if (test_port == MAXDATAPORT)
    {
      close (sock);
      sock = -1;
      return -1;
    }

  if (listen (sock, 1))
    {
      close (sock);
      sock = -1;
      return -1;
    }

  setPort (test_port);

  conn_state = CONN_CONNECTING;
  return 0;
}

int
Rts2DevConnData::send (char *message)
{
  return 0;
}

int
Rts2DevConnData::send (char *data, size_t data_size)
{
  if (conn_state != CONN_CONNECTED)
    return -2;
  return write (sock, data, data_size);
}

int
Rts2DevConnData::sendHeader ()
{

}

int
Rts2DevConnData::acceptConn ()
{
  int new_sock;
  struct sockaddr_in other_side;
  socklen_t addr_size = sizeof (struct sockaddr_in);
  new_sock = accept (sock, (struct sockaddr *) &other_side, &addr_size);
  if (new_sock == -1)
    {
      syslog (LOG_ERR, "Rts2DevConnData::acceptConn error %m");
      return -1;
    }
  else
    {
      close (sock);
      sock = new_sock;
      syslog (LOG_DEBUG, "Rts2DevConnData::acceptConn connection accepted");
      conn_state = CONN_CONNECTED;
      return 0;
    }
}

void
Rts2State::setState (int new_state, char *description)
{
  // state was set..do not set it again
  if (state == new_state)
    {
      return;
    }
  state = new_state;
  syslog (LOG_DEBUG, "Rts2State::setState new_state: %i desc: %s this: %p",
	  new_state, description, this);
  master->sendStatusMessage (state_name, state);
};

void
Rts2State::maskState (int state_mask, int new_state, char *description)
{
  int masked_state = state;
  masked_state &= !state_mask;
  masked_state |= new_state;
  setState (masked_state, description);
}

int
Rts2State::sendInfo (Rts2Conn * conn, int state_num)
{
  int ret;
  char *msg;
  asprintf (&msg, "I status %i %s", state_num, state_name);
  ret = conn->sendValue (msg, state);
  free (msg);
  return ret;
}

Rts2Device::Rts2Device (int in_argc, char **in_argv, int in_device_type, int default_port, char *default_name):
Rts2Block (in_argc, in_argv)
{
  /* put defaults to variables.. */
  setPort (default_port);
  device_name = default_name;
  centrald_host = "localhost";
  centrald_port = 5557;
  log_option = 0;

  statesSize = 0;
  states = NULL;

  device_type = in_device_type;

  device_host = NULL;

  // now add options..
  addOption ('p', "port", 1, "port to listen for request");
  addOption ('l', "hostname", 1,
	     "hostname, if it different from return of gethostname()");
  addOption ('s', "centrald_host", 1,
	     "name of computer, on which central server runs");
  addOption ('q', "centrald_port", 1, "port number of central host");
  addOption ('d', "device_name", 1, "name of device");
  addOption ('e', "log_stderr", 0,
	     "logs also to stderr (not only to syslogd)");
}

Rts2Device::~Rts2Device (void)
{
  int i;

  close (lockf);

  for (i = 0; i < statesSize; i++)
    delete states[i];
  free (states);
  states = NULL;
}

void
Rts2Device::setStateNames (int in_states_size, char **states_names)
{
  if (in_states_size == 0)
    return;

  int i;
  char *state_name = *states_names;
  syslog (LOG_DEBUG, "Rts2Device::setStateNames states: %i\n",
	  in_states_size);
  states = (Rts2State **) malloc (sizeof (Rts2State *) * in_states_size);
  for (i = 0; i < in_states_size; i++)
    {
      states[i] = new Rts2State (this, state_name);
      state_name++;
    }
  statesSize = in_states_size;
}

int
Rts2Device::processOption (int in_opt)
{
  switch (in_opt)
    {
    case 'p':
      int a_port;
      a_port = atoi (optarg);
      setPort (a_port);
      break;
    case 'l':
      device_host = optarg;
      break;
    case 's':
      centrald_host = optarg;
      break;
    case 'q':
      centrald_port = atoi (optarg);
      break;
    case 'd':
      device_name = optarg;
      break;
    case 'e':
      log_option |= LOG_PERROR;
      break;
    default:
      return Rts2Block::processOption (in_opt);
    }
  return 0;
}

Rts2Conn *
Rts2Device::createClientConnection (char *in_device_name)
{
  return new Rts2Dev2DevConn (this, in_device_name);
}

Rts2Conn *
Rts2Device::createClientConnection (Rts2Address * in_addres)
{
  return new Rts2Dev2DevConn (this, in_addres);
}

int
Rts2Device::changeState (int state_num, int new_state, char *description)
{
  states[state_num]->setState (new_state, description);
  return 0;
}

int
Rts2Device::maskState (int state_num, int state_mask, int new_state,
		       char *description)
{
  syslog (LOG_DEBUG,
	  "Rts2Device::maskState state: %i state_mask: %i new_state: %i desc: %s",
	  state_num, state_mask, new_state, description);
  states[state_num]->maskState (state_mask, new_state, description);
}

int
Rts2Device::init ()
{
  int ret;
  char *lock_fname;
  FILE *lock_file;

  // try to open log file..

  ret = Rts2Block::init ();

  openlog (NULL, log_option, LOG_LOCAL0);

  asprintf (&lock_fname, "/var/run/rts2_%s", device_name);

  lockf = open (lock_fname, O_RDWR | O_CREAT);

  if (lockf == -1)
    {
      syslog (LOG_ERR, "cannot open lock file %s", lock_fname);
      return -1;
    }

  ret = flock (lockf, LOCK_EX | LOCK_NB);
  if (ret)
    {
      if (errno == EWOULDBLOCK)
	{
	  syslog (LOG_ERR, "lock file %s owned by another process",
		  lock_fname);
	  return -1;
	}
      syslog (LOG_ERR, "cannot flock %s: %m, lock_fname");
      return -1;
    }

  lock_file = fdopen (lockf, "w+");

  free (lock_fname);

  fprintf (lock_file, "%i\n", getpid ());

  fflush (lock_file);

  conn_master =
    new Rts2DevConnMaster (this, device_host, getPort (), device_name,
			   device_type, centrald_host, centrald_port);
  connections[0] = conn_master;

  while (connections[0]->init () < 0)
    {
      syslog (LOG_DEBUG, "Rts2Device::init waiting for master");
      sleep (60);
    }
  return ret;
}

int
Rts2Device::idle ()
{
  Rts2Block::idle ();
}

int
Rts2Device::authorize (Rts2DevConn * conn)
{
  return conn_master->authorize (conn);
}

int
Rts2Device::sendStatusInfo (Rts2DevConn * conn)
{
  int i;
  int ret;

  // last state holds priority info
  ret = conn->sendValue ("I status_num", statesSize + 1);

  if (ret)
    return ret;
  for (i = 0; i < statesSize; i++)
    {
      ret = states[i]->sendInfo (conn, i);
      if (ret)
	return ret;
    }
  return conn->sendPriorityInfo (i);
}

int
Rts2Device::ready ()
{
  return -1;
}

int
Rts2Device::info ()
{
  return -1;
}

int
Rts2Device::baseInfo ()
{
  return -1;
}

int
Rts2Device::ready (Rts2Conn * conn)
{
  int ret;
  ret = ready ();
  if (ret)
    {
      conn->sendCommandEnd (DEVDEM_E_HW, "device not ready");
      return -1;
    }
  return 0;
}

int
Rts2Device::info (Rts2Conn * conn)
{
  int ret;
  ret = info ();
  if (ret)
    {
      conn->sendCommandEnd (DEVDEM_E_HW, "device not ready");
      return -1;
    }
  return 0;
}

int
Rts2Device::baseInfo (Rts2Conn * conn)
{
  int ret;
  ret = baseInfo ();
  if (ret)
    {
      conn->sendCommandEnd (DEVDEM_E_HW, "device not ready");
      return -1;
    }
  return 0;
}
