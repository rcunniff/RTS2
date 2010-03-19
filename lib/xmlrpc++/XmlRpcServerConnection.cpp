
#include "XmlRpcServerConnection.h"

#include "XmlRpcSocket.h"
#include "XmlRpc.h"
#include "base64.h"
#include <sstream>

#ifndef MAKEDEPEND
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
#endif

using namespace XmlRpc;

// Static data
const char XmlRpcServerConnection::METHODNAME_TAG[] = "<methodName>";
const char XmlRpcServerConnection::PARAMS_TAG[] = "<params>";
const char XmlRpcServerConnection::PARAMS_ETAG[] = "</params>";
const char XmlRpcServerConnection::PARAM_TAG[] = "<param>";
const char XmlRpcServerConnection::PARAM_ETAG[] = "</param>";

const std::string XmlRpcServerConnection::SYSTEM_MULTICALL = "system.multicall";
const std::string XmlRpcServerConnection::METHODNAME = "methodName";
const std::string XmlRpcServerConnection::PARAMS = "params";

const std::string XmlRpcServerConnection::FAULTCODE = "faultCode";
const std::string XmlRpcServerConnection::FAULTSTRING = "faultString";

// The server delegates handling client requests to a serverConnection object.
XmlRpcServerConnection::XmlRpcServerConnection(int fd, XmlRpcServer* server, bool deleteOnClose /*= false*/) : XmlRpcSource(fd, deleteOnClose)
{
	XmlRpcUtil::log(2,"XmlRpcServerConnection: new socket %d.", fd);

	_header_buf = NULL;
	_header_length = 0;

	_request_buf = NULL;
	_request_length = 0;

	_server = server;
	_connectionState = READ_HEADER;
	_keepAlive = true;

	_get_response_header = std::string ("");
	_extra_headers.clear ();

	_get_response_length = 0;
	_get_response = NULL;
}

XmlRpcServerConnection::~XmlRpcServerConnection()
{
	XmlRpcUtil::log(4,"XmlRpcServerConnection dtor.");
	_server->removeConnection(this);
}

// Handle input on the server socket by accepting the connection
// and reading the rpc request. Return true to continue to monitor
// the socket for events, false to remove it from the dispatcher.
unsigned XmlRpcServerConnection::handleEvent(unsigned /*eventType*/)
{
	if (_connectionState == READ_HEADER)
		if ( ! readHeader()) return 0;

	if (_connectionState == READ_REQUEST)
		if ( ! readRequest()) return 0;

	if (_connectionState == GET_REQUEST)
		if ( ! handleGet()) return 0;

	if (_connectionState == POST_REQUEST)
	  	if ( ! handlePost()) return 0;

	if (_connectionState == WRITE_RESPONSE)
		if ( ! writeResponse()) return 0;

	return (_connectionState == WRITE_RESPONSE)
		? XmlRpcDispatch::WritableEvent : XmlRpcDispatch::ReadableEvent;
}

bool XmlRpcServerConnection::readHeader()
{
	// Read available data
	bool eof;
	if ( ! XmlRpcSocket::nbRead(this->getfd(), _header_buf, _header_length, &eof))
	{
		// Its only an error if we already have read some data
		if (_header_length > 0)
			XmlRpcUtil::error("XmlRpcServerConnection::readHeader: error while reading header (%s).",XmlRpcSocket::getErrorMsg().c_str());
		return false;
	}

	if (_header_length == 0)
		_header = std::string ("");
	else
		_header = std::string (_header_buf);
	XmlRpcUtil::log(4, "XmlRpcServerConnection::readHeader: read %d bytes.", _header.length());
								 // Start of header
	char *hp = _header_buf;
								 // End of string
	char *ep = hp + _header_length;
	char *gp = 0;				 // GET method pointer
	char *pp = 0;				 // POST method pointer
	char *bp = 0;				 // Start of body
	char *lp = 0;				 // Start of content-length value
	char *kp = 0;				 // Start of connection value
	char *ap = 0;				 // Start of authorization header

	for (char *cp = hp; (bp == 0) && (cp < ep); ++cp)
	{
		if ((ep - cp > 4) && (strncasecmp(cp, "GET ", 4) == 0))
			gp = cp + 4;
		if ((ep - cp > 5) && (strncasecmp(cp, "POST ", 5) == 0))
		  	pp = cp + 5;
		else if ((ep - cp > 16) && (strncasecmp(cp, "Content-length: ", 16) == 0))
			lp = cp + 16;
		else if ((ep - cp > 12) && (strncasecmp(cp, "Connection: ", 12) == 0))
			kp = cp + 12;
		else if ((ep - cp > 12) && (strncasecmp (cp, "Authorization: ", 15) == 0))
			ap = cp + 15;
		else if ((ep - cp >= 4) && (strncmp(cp, "\r\n\r\n", 4) == 0))
			bp = cp + 4;
		else if ((ep - cp >= 2) && (strncmp(cp, "\n\n", 2) == 0))
			bp = cp + 2;
	}

	// If we haven't gotten the entire header yet, return (keep reading)
	if (bp == 0)
	{
		// EOF in the middle of a request is an error, otherwise its ok
		if (eof)
		{
			XmlRpcUtil::log(4, "XmlRpcServerConnection::readHeader: EOF");
			if (_header_length > 0)
				XmlRpcUtil::error("XmlRpcServerConnection::readHeader: EOF while reading header");
			return false;		 // Either way we close the connection
		}

		return true;			 // Keep reading
	}

	// authorization..
	if (ap != 0)
	{
		while (isspace(*ap))
			ap++;
		// user..
		if (ep - ap > 5 && strncmp (ap, "Basic", 5) == 0)
		{
			ap += 5;
			while (isspace (*ap))
				ap++;
			char *ape = ap;
			while (!(isspace (*ape) || (*ape == '\n') || (*ape == '\r')))
				ape++;
			std::string t_ap = _header.substr (ap - hp, ape - ap);

			int iostatus = 0;
			base64<char> decoder;
			std::back_insert_iterator<std::string> ins = std::back_inserter(_authorization);
			decoder.get(t_ap.begin (), t_ap.end (), ins, iostatus);

		}
	}

	// XML-RPC requests are POST. If we received GET request, then get request string and call it a day..
	if (gp != 0)
	{
		while (isspace(*gp))
			gp++;
		char *cp = gp;
		while (! (cp >= ep || isspace(*cp) || *cp == '\r' || *cp == '\n'))
			cp++;
		_get = _header.substr(gp - hp, cp - gp);
		XmlRpcUtil::log(4, "XmlRpcServerConnection::readHeader: GET request for %s", _get.c_str());

		_connectionState = GET_REQUEST;

		return true;
	}

	// decode post request
	if (pp != 0)
	{
		while (isspace(*pp))
			pp++;
		char *cp = pp;
		while (! (cp >= ep || isspace(*cp) || *cp == '\r' || *cp == '\n'))
			cp++;
		_post = _header.substr(pp - hp, cp - pp);
		XmlRpcUtil::log(4, "XmlRpcServerConnection::readHeader: POST request for %s", _post.c_str());
	}

	// Decode content length
	if (lp == 0)
	{
		XmlRpcUtil::error("XmlRpcServerConnection::readHeader: No Content-length specified");
		return false;			 // We could try to figure it out by parsing as we read, but for now...
	}

	_contentLength = atoi(lp);
	if (_contentLength <= 0)
	{
		XmlRpcUtil::error("XmlRpcServerConnection::readHeader: Invalid Content-length specified (%d).", _contentLength);
		return false;
	}

	XmlRpcUtil::log(3, "XmlRpcServerConnection::readHeader: specified content length is %d.", _contentLength);

	// Otherwise copy non-header data to request buffer and set state to read request.
	_request_length = ep - bp;
	_request_buf = (char*) malloc(_request_length + 1);
	memcpy(_request_buf, bp, _request_length);
	// NULL terminate it..
	_request_buf[_request_length] = '\0';
	_request = _request_buf;

	XmlRpcUtil::log(3, "XmlRpcServerConnection::readHeader: request %s", _request_buf);

	// Parse out any interesting bits from the header (HTTP version, connection)
	_keepAlive = true;
	if (_header.find("HTTP/1.0") != std::string::npos)
	{
		if (kp == 0 || strncasecmp(kp, "keep-alive", 10) != 0)
			_keepAlive = false;	 // Default for HTTP 1.0 is to close the connection
	}
	else
	{
		if (kp != 0 && strncasecmp(kp, "close", 5) == 0)
			_keepAlive = false;
	}
	XmlRpcUtil::log(3, "KeepAlive: %d", _keepAlive);

	_header = "";
	free(_header_buf);
	_header_buf = NULL;
	_header_length = 0;
	if (_post == std::string("/RPC2"))
		_connectionState = READ_REQUEST;
	else
		_connectionState = POST_REQUEST;
	return true;				 // Continue monitoring this source
}

bool XmlRpcServerConnection::readRequest()
{
	// If we dont have the entire request yet, read available data
	if (_request_length < _contentLength)
	{
		bool eof;
		if ( ! XmlRpcSocket::nbRead(this->getfd(), _request_buf, _request_length, &eof))
		{
			XmlRpcUtil::error("XmlRpcServerConnection::readRequest: read error (%s).",XmlRpcSocket::getErrorMsg().c_str());
			return false;
		}

		// If we haven't gotten the entire request yet, return (keep reading)
		if (_request_length < _contentLength)
		{
			if (eof)
			{
				XmlRpcUtil::error("XmlRpcServerConnection::readRequest: EOF while reading request");
				return false;	 // Either way we close the connection
			}
			return true;
		}
	}

	// Otherwise, parse and dispatch the request
	XmlRpcUtil::log(3, "XmlRpcServerConnection::readRequest read %d bytes.", _request_length);
	//XmlRpcUtil::log(5, "XmlRpcServerConnection::readRequest:\n%s\n", _request.c_str());

	_connectionState = WRITE_RESPONSE;

	_request = _request_buf;

	return true;				 // Continue monitoring this source
}

bool XmlRpcServerConnection::handleGet()
{
	if (_get_response_header.length () == 0 || _get_response_length == 0)
	{
		executeGet();
		_getHeaderWritten = 0;
		_getWritten = 0;
		_bytesWritten = 0;
		if (_get_response_header.length () == 0 || _get_response_length == 0)
		{
			XmlRpcUtil::error("XmlRpcServerConnection::handleGet: empty response.");
			return false;
		}
	}

	if (_getHeaderWritten != _get_response_header.length ())
	{
		if ( ! XmlRpcSocket::nbWriteBuf(this->getfd(), _get_response_header.c_str () + _getHeaderWritten, _get_response_header.length (), &_getHeaderWritten))
		{
			XmlRpcUtil::error("XmlRpcServerConnection::handleGet: write error (%s).",XmlRpcSocket::getErrorMsg().c_str());
			return false;
		}
		XmlRpcUtil::log(3, "XmlRpcServerConnection::handleGet: wrote %d of %d bytes.", _getHeaderWritten, _get_response_header.length ());
	}
	if (_getHeaderWritten == _get_response_header.length () && _getWritten != _get_response_length)
	{
		if ( ! XmlRpcSocket::nbWriteBuf(this->getfd(), _get_response, _get_response_length, &_getWritten))
		{
			XmlRpcUtil::error("XmlRpcServerConnection::handleGet: write error (%s).",XmlRpcSocket::getErrorMsg().c_str());
			return false;
		}
		XmlRpcUtil::log(3, "XmlRpcServerConnection::handleGet: wrote %d of %d bytes.", _getWritten, _get_response_length);
	}

	// Prepare to read the next request
	if (_getHeaderWritten == _get_response_header.length () && _getWritten == _get_response_length)
	{
		_authorization = "";
		_get = "";
		_post = "";
		_header = "";
		if (_header_buf)
			free(_header_buf);
		_header_buf  = NULL;
		_header_length = 0;
		_request = "";
		if (_request_buf)
			free(_request_buf);
		_request_buf = NULL;
		_request_length = 0;
		_get_response_header = std::string ();
		_extra_headers.clear ();
		delete[] _get_response;
		
		_get_response_length = 0;
		_get_response = NULL;
		_response = "";
		_connectionState = READ_HEADER;
	}

	return _keepAlive;			 // Continue monitoring this source if true
}

bool XmlRpcServerConnection::handlePost()
{
	_get = _post;
	return handleGet();
}

bool XmlRpcServerConnection::writeResponse()
{
	if (_response.length() == 0)
	{
		executeRequest();
		_bytesWritten = 0;
		if (_response.length() == 0)
		{
			XmlRpcUtil::error("XmlRpcServerConnection::writeResponse: empty response.");
			return false;
		}
	}

	// Try to write the response
	if ( ! XmlRpcSocket::nbWrite(this->getfd(), _response, &_bytesWritten))
	{
		XmlRpcUtil::error("XmlRpcServerConnection::writeResponse: write error (%s).",XmlRpcSocket::getErrorMsg().c_str());
		return false;
	}
	XmlRpcUtil::log(3, "XmlRpcServerConnection::writeResponse: wrote %d of %d bytes.", _bytesWritten, _response.length());

	// Prepare to read the next request
	if (_bytesWritten == int(_response.length()))
	{
		_authorization = "";
		_get = "";
		_post = "";
		_header = "";
		if (_header_buf)
			free(_header_buf);
		_header_buf = NULL;
		_header_length = 0;
		_request = "";
		if (_request_buf)
			free(_request_buf);
		_request_buf = NULL;
		_request_length = 0;
		_get_response_header = std::string ("");
		_extra_headers.clear ();
		_get_response_length = 0;
		_get_response = NULL;
		_response = "";
		_connectionState = READ_HEADER;
	}

	return _keepAlive;			 // Continue monitoring this source if true
}

// Run the method, generate _response string
void XmlRpcServerConnection::executeRequest()
{
	XmlRpcValue params, resultValue;
	std::string methodName = parseRequest(params);
	XmlRpcUtil::log(2, "XmlRpcServerConnection::executeRequest: server calling method '%s'", methodName.c_str());

	try
	{
		if ( ! executeMethod(methodName, params, resultValue) &&
			! executeMulticall(methodName, params, resultValue))
			generateFaultResponse(methodName + ": unknown method name");
		else
			generateResponse(resultValue.toXml());
	}
	catch (const XmlRpcException& fault)
	{
		XmlRpcUtil::log(2, "XmlRpcServerConnection::executeRequest: fault %s.",
			fault.getMessage().c_str());
		generateFaultResponse(fault.getMessage(), fault.getCode());
	}
}

// Run the method, generate _get_response buffer, fill _get_response_length
void XmlRpcServerConnection::executeGet()
{
	const char* response_type = "text/plain";

	int http_code = HTTP_BAD_REQUEST;
	const char *http_code_string = "Failed";

	XmlRpcServerGetRequest* request = _server->findGetRequest(_get);
	if (request == NULL)
	{
	  	// if we are on top page, generate list of pages..
		if (_get == "/" || _get == "")
		{
			std::ostringstream oss;
			oss << "<html><head><title>List of path available on the server</title></head>"
				<< std::endl << "<body><p>Those requests are available on the server:</p>";

			for (RequestMap::const_iterator riter = _server->requestsBegin (); riter != _server->requestsEnd (); riter++)
			{
				if (riter->second->getDescription () != NULL)
					oss << "<a href='" << riter->second->getPrefix () << "/'>" << riter->second->getDescription () << "</a><br>" << std::endl;
			}

			oss << std::endl << "</body></html>";
			
			response_type = "text/html";
			_get_response_length = oss.str ().length ();
			_get_response = new char[_get_response_length];
			memcpy (_get_response, oss.str ().c_str (), _get_response_length);

			http_code = HTTP_OK;
		}
		else
		{
			XmlRpcUtil::log(2, "XmlRpcServerConnection::executeGet: cannot find request for prefix %s", _get.c_str());
			http_code = HTTP_BAD_REQUEST;
		}
	}
	else
	{
		request->setAuthorization (_authorization);

		HttpParams params = HttpParams ();
	
		try
		{
			std::string path = _get.substr (request->getPrefix ().length ()).c_str ();
			// if there are any parameters..
			std::string::size_type pi = path.find ('?');
			if (pi != std::string::npos)
			{
				params.parse (path.substr (pi + 1));
				path = path.substr (0, pi);
			}
			// check for ..
			if (path.find ("..") != std::string::npos)
				throw XmlRpcException ("Path contains ..");
			if (path.find ("!") != std::string::npos)
				throw XmlRpcException ("Path contains !");
			
			// add params from _request for POST requests
			if (_connectionState == POST_REQUEST)
			{
				params.parse (_request);
			}

			request->setConnection (this);

			request->execute (path, &params, http_code, response_type, _get_response, _get_response_length);
		}
		catch (const std::exception& ex)
		{
			_get_response = new char[501];
			response_type = "text/html";
			_get_response_length = snprintf (_get_response, 500, "<html><head><title>Error</title></head><body><p>Bad request %s</p></body></html>", ex.what());

			http_code = HTTP_BAD_REQUEST;
		}
	}

	switch (http_code)
	{
		case HTTP_OK:
			http_code_string = "OK";
			break;
		case HTTP_UNAUTHORIZED:
			http_code_string = "Authorization Required";
			addExtraHeader ("WWW-Authenticate", "Basic realm=\"Your RTS2 login\"");
			break;
		case HTTP_BAD_REQUEST:
		default:
			http_code_string = "Failed";
			break;
	}

	std::ostringstream _os;

	_os << "HTTP/1.0 " << http_code << " " << http_code_string
		<< "\r\nServer: XMLRCP\r\nContent-Type: " << response_type
		<< "\r\nContent-length: " << _get_response_length;

	for (std::list <std::pair <const char*, const char*> >::iterator iter = _extra_headers.begin (); iter != _extra_headers.end (); iter++)
	  	_os << "\r\n" << iter->first << ": " << iter->second;
	
	_os << "\r\n\r\n";

	_get_response_header = _os.str ();
	printf ("%s", _get_response_header.c_str ());
}

// Parse the method name and the argument values from the request.
std::string XmlRpcServerConnection::parseRequest(XmlRpcValue& params)
{
	int offset = 0;				 // Number of chars parsed from the request

	std::string methodName = XmlRpcUtil::parseTag(METHODNAME_TAG, _request, &offset);

	if (methodName.size() > 0 && XmlRpcUtil::findTag(PARAMS_TAG, _request, &offset))
	{
		int nArgs = 0;
		while (XmlRpcUtil::nextTagIs(PARAM_TAG, _request, &offset))
		{
			params[nArgs++] = XmlRpcValue(_request, &offset);
			(void) XmlRpcUtil::nextTagIs(PARAM_ETAG, _request, &offset);
		}

		(void) XmlRpcUtil::nextTagIs(PARAMS_ETAG, _request, &offset);
	}

	return methodName;
}

// Execute a named method with the specified params.
bool XmlRpcServerConnection::executeMethod(const std::string& methodName, XmlRpcValue& params, XmlRpcValue& result)
{
	XmlRpcServerMethod* method = _server->findMethod(methodName);

	if ( ! method) return false;

	method->setAuthorization (_authorization);

	method->execute (params, result);

	// Ensure a valid result value
	if ( ! result.valid ())
		result = std::string ();

	return true;
}

// Execute multiple calls and return the results in an array.
bool XmlRpcServerConnection::executeMulticall(const std::string& in_methodName, XmlRpcValue& params, XmlRpcValue& result)
{
	if (in_methodName != SYSTEM_MULTICALL) return false;

	// There ought to be 1 parameter, an array of structs
	if (params.size() != 1 || params[0].getType() != XmlRpcValue::TypeArray)
		throw XmlRpcException(SYSTEM_MULTICALL + ": Invalid argument (expected an array)");

	int nc = params[0].size();
	result.setSize(nc);

	for (int i=0; i<nc; ++i)
	{
		if ( ! params[0][i].hasMember(METHODNAME) ||
			! params[0][i].hasMember(PARAMS))
		{
			result[i][FAULTCODE] = -1;
			result[i][FAULTSTRING] = SYSTEM_MULTICALL +
				": Invalid argument (expected a struct with members methodName and params)";
			continue;
		}

		const std::string& methodName = params[0][i][METHODNAME];
		XmlRpcValue& methodParams = params[0][i][PARAMS];

		XmlRpcValue resultValue;
		resultValue.setSize(1);
		try
		{
			if ( ! executeMethod(methodName, methodParams, resultValue[0]) &&
				! executeMulticall(methodName, params, resultValue[0]))
			{
				result[i][FAULTCODE] = -1;
				result[i][FAULTSTRING] = methodName + ": unknown method name";
			}
			else
				result[i] = resultValue;

		}
		catch (const XmlRpcException& fault)
		{
			result[i][FAULTCODE] = fault.getCode();
			result[i][FAULTSTRING] = fault.getMessage();
		}
	}

	return true;
}

// Create a response from results xml
void XmlRpcServerConnection::generateResponse(std::string const& resultXml)
{
	const char RESPONSE_1[] =
		"<?xml version=\"1.0\"?>\r\n"
		"<methodResponse><params><param>\r\n\t";
	const char RESPONSE_2[] =
		"\r\n</param></params></methodResponse>\r\n";

	std::string body = RESPONSE_1 + resultXml + RESPONSE_2;
	std::string header = generateHeader(body);

	_response = header + body;
	XmlRpcUtil::log(5, "XmlRpcServerConnection::generateResponse:\n%s\n", _response.c_str());
}

// Prepend http headers
std::string XmlRpcServerConnection::generateHeader(std::string const& body)
{
	std::string header =
		"HTTP/1.1 200 OK\r\n"
		"Server: ";
	header += XMLRPC_VERSION;
	header += "\r\n"
		"Content-Type: text/xml\r\n"
		"Content-length: ";

	char buffLen[40];
	sprintf(buffLen,"%i\r\n\r\n", (int) body.size());

	return header + buffLen;
}

void XmlRpcServerConnection::generateFaultResponse(std::string const& errorMsg, int errorCode)
{
	const char RESPONSE_1[] =
		"<?xml version=\"1.0\"?>\r\n"
		"<methodResponse><fault>\r\n\t";
	const char RESPONSE_2[] =
		"\r\n</fault></methodResponse>\r\n";

	XmlRpcValue faultStruct;
	faultStruct[FAULTCODE] = errorCode;
	faultStruct[FAULTSTRING] = errorMsg;
	std::string body = RESPONSE_1 + faultStruct.toXml() + RESPONSE_2;
	std::string header = generateHeader(body);

	_response = header + body;
}
