#!/usr/bin/env python2.7
# -*- coding: utf-8 -*-
""":synopsis: FTP protocol client (requires sockets).


"""
class FTP:


	"""
	Return a new instance of the :class:`FTP` class.  When *host* is given, the
	method call ``connect(host)`` is made.  When *user* is given, additionally
	the method call ``login(user, passwd, acct)`` is made (where *passwd* and
	*acct* default to the empty string when not given).  The optional *timeout*
	parameter specifies a timeout in seconds for blocking operations like the
	connection attempt (if is not specified, the global default timeout setting
	will be used).
	
	"""
	
	
	def __init__(self, ):
		pass
	
	def set_debuglevel(self, level):
		"""
		Set the instance's debugging level.  This controls the amount of debugging
		output printed.  The default, ``0``, produces no debugging output.  A value of
		``1`` produces a moderate amount of debugging output, generally a single line
		per request.  A value of ``2`` or higher produces the maximum amount of
		debugging output, logging each line sent and received on the control connection.
		
		
		"""
		pass
		
	def connect(self, host,port,timeout):
		"""
		Connect to the given host and port.  The default port number is ``21``, as
		specified by the FTP protocol specification.  It is rarely needed to specify a
		different port number.  This function should be called only once for each
		instance; it should not be called at all if a host was given when the instance
		was created.  All other methods can only be used after a connection has been
		made.
		
		The optional *timeout* parameter specifies a timeout in seconds for the
		connection attempt. If no *timeout* is passed, the global default timeout
		setting will be used.
		
		"""
		pass
		
	def getwelcome(self, ):
		"""
		Return the welcome message sent by the server in reply to the initial
		connection.  (This message sometimes contains disclaimers or help information
		that may be relevant to the user.)
		
		
		"""
		pass
		
	def login(self, user,passwd,acct):
		"""
		Log in as the given *user*.  The *passwd* and *acct* parameters are optional and
		default to the empty string.  If no *user* is specified, it defaults to
		``'anonymous'``.  If *user* is ``'anonymous'``, the default *passwd* is
		``'anonymous@'``.  This function should be called only once for each instance,
		after a connection has been established; it should not be called at all if a
		host and user were given when the instance was created.  Most FTP commands are
		only allowed after the client has logged in.  The *acct* parameter supplies
		"accounting information"; few systems implement this.
		
		
		"""
		pass
		
	def abort(self, ):
		"""
		Abort a file transfer that is in progress.  Using this does not always work, but
		it's worth a try.
		
		
		"""
		pass
		
	def sendcmd(self, command):
		"""
		Send a simple command string to the server and return the response string.
		
		
		"""
		pass
		
	def voidcmd(self, command):
		"""
		Send a simple command string to the server and handle the response.  Return
		nothing if a response code corresponding to success (codes in the range
		200--299) is received.  Raise :exc:`error_reply` otherwise.
		
		
		"""
		pass
		
	def retrbinary(self, command,callback,maxblocksize,rest):
		"""
		Retrieve a file in binary transfer mode.  *command* should be an appropriate
		``RETR`` command: ``'RETR filename'``. The *callback* function is called for
		each block of data received, with a single string argument giving the data
		block. The optional *maxblocksize* argument specifies the maximum chunk size to
		read on the low-level socket object created to do the actual transfer (which
		will also be the largest size of the data blocks passed to *callback*).  A
		reasonable default is chosen. *rest* means the same thing as in the
		:meth:`transfercmd` method.
		
		
		"""
		pass
		
	def retrlines(self, command,callback):
		"""
		Retrieve a file or directory listing in ASCII transfer mode.  *command*
		should be an appropriate ``RETR`` command (see :meth:`retrbinary`) or a
		command such as ``LIST``, ``NLST`` or ``MLSD`` (usually just the string
		``'LIST'``).  ``LIST`` retrieves a list of files and information about those files.
		``NLST`` retrieves a list of file names.  On some servers, ``MLSD`` retrieves
		a machine readable list of files and information about those files.  The *callback*
		function is called for each line with a string argument containing the line with
		the trailing CRLF stripped.  The default *callback* prints the line to ``sys.stdout``.
		
		
		"""
		pass
		
	def set_pasv(self, boolean):
		"""
		Enable "passive" mode if *boolean* is true, other disable passive mode.  (In
		Python 2.0 and before, passive mode was off by default; in Python 2.1 and later,
		it is on by default.)
		
		
		"""
		pass
		
	def storbinary(self, command,file,blocksize,callback,rest):
		"""
		Store a file in binary transfer mode.  *command* should be an appropriate
		``STOR`` command: ``"STOR filename"``. *file* is an open file object which is
		read until EOF using its :meth:`read` method in blocks of size *blocksize* to
		provide the data to be stored.  The *blocksize* argument defaults to 8192.
		*callback* is an optional single parameter callable that is called
		on each block of data after it is sent. *rest* means the same thing as in
		the :meth:`transfercmd` method.
		
		"""
		pass
		
	def storlines(self, command,file,callback):
		"""
		Store a file in ASCII transfer mode.  *command* should be an appropriate
		``STOR`` command (see :meth:`storbinary`).  Lines are read until EOF from the
		open file object *file* using its :meth:`readline` method to provide the data to
		be stored.  *callback* is an optional single parameter callable
		that is called on each line after it is sent.
		
		"""
		pass
		
	def transfercmd(self, cmd,rest):
		"""
		Initiate a transfer over the data connection.  If the transfer is active, send a
		``EPRT`` or  ``PORT`` command and the transfer command specified by *cmd*, and
		accept the connection.  If the server is passive, send a ``EPSV`` or ``PASV``
		command, connect to it, and start the transfer command.  Either way, return the
		socket for the connection.
		
		If optional *rest* is given, a ``REST`` command is sent to the server, passing
		*rest* as an argument.  *rest* is usually a byte offset into the requested file,
		telling the server to restart sending the file's bytes at the requested offset,
		skipping over the initial bytes.  Note however that RFC 959 requires only that
		*rest* be a string containing characters in the printable range from ASCII code
		33 to ASCII code 126.  The :meth:`transfercmd` method, therefore, converts
		*rest* to a string, but no check is performed on the string's contents.  If the
		server does not recognize the ``REST`` command, an :exc:`error_reply` exception
		will be raised.  If this happens, simply call :meth:`transfercmd` without a
		*rest* argument.
		
		
		"""
		pass
		
	def ntransfercmd(self, cmd,rest):
		"""
		Like :meth:`transfercmd`, but returns a tuple of the data connection and the
		expected size of the data.  If the expected size could not be computed, ``None``
		will be returned as the expected size.  *cmd* and *rest* means the same thing as
		in :meth:`transfercmd`.
		
		
		"""
		pass
		
	def nlst(self, argument,more):
		"""
		Return a list of file names as returned by the ``NLST`` command.  The
		optional *argument* is a directory to list (default is the current server
		directory).  Multiple arguments can be used to pass non-standard options to
		the ``NLST`` command.
		
		
		"""
		pass
		
	def dir(self, argument,more):
		"""
		Produce a directory listing as returned by the ``LIST`` command, printing it to
		standard output.  The optional *argument* is a directory to list (default is the
		current server directory).  Multiple arguments can be used to pass non-standard
		options to the ``LIST`` command.  If the last argument is a function, it is used
		as a *callback* function as for :meth:`retrlines`; the default prints to
		``sys.stdout``.  This method returns ``None``.
		
		
		"""
		pass
		
	def rename(self, _fromname,toname):
		"""
		Rename file *fromname* on the server to *toname*.
		
		
		"""
		pass
		
	def delete(self, filename):
		"""
		Remove the file named *filename* from the server.  If successful, returns the
		text of the response, otherwise raises :exc:`error_perm` on permission errors or
		:exc:`error_reply` on other errors.
		
		
		"""
		pass
		
	def cwd(self, pathname):
		"""
		Set the current directory on the server.
		
		
		"""
		pass
		
	def mkd(self, pathname):
		"""
		Create a new directory on the server.
		
		
		"""
		pass
		
	def pwd(self, ):
		"""
		Return the pathname of the current directory on the server.
		
		
		"""
		pass
		
	def rmd(self, dirname):
		"""
		Remove the directory named *dirname* on the server.
		
		
		"""
		pass
		
	def size(self, filename):
		"""
		Request the size of the file named *filename* on the server.  On success, the
		size of the file is returned as an integer, otherwise ``None`` is returned.
		Note that the ``SIZE`` command is not  standardized, but is supported by many
		common server implementations.
		
		
		"""
		pass
		
	def quit(self, ):
		"""
		Send a ``QUIT`` command to the server and close the connection. This is the
		"polite" way to close a connection, but it may raise an exception if the server
		responds with an error to the ``QUIT`` command.  This implies a call to the
		:meth:`close` method which renders the :class:`FTP` instance useless for
		subsequent calls (see below).
		
		
		"""
		pass
		
	def close(self, ):
		"""
		Close the connection unilaterally.  This should not be applied to an already
		closed connection such as after a successful call to :meth:`quit`.  After this
		call the :class:`FTP` instance should not be used any more (after a call to
		:meth:`close` or :meth:`quit` you cannot reopen the connection by issuing
		another :meth:`login` method).
		
		
		FTP_TLS Objects
		---------------
		
		:class:`FTP_TLS` class inherits from :class:`FTP`, defining these additional objects:
		
		"""
		pass
		
	


class FTP_TLS:


	"""
	A :class:`FTP` subclass which adds TLS support to FTP as described in
	:rfc:`4217`.
	Connect as usual to port 21 implicitly securing the FTP control connection
	before authenticating. Securing the data connection requires the user to
	explicitly ask for it by calling the :meth:`prot_p` method.
	*keyfile* and *certfile* are optional -- they can contain a PEM formatted
	private key and certificate chain file name for the SSL connection.
	
	"""
	
	
	def __init__(self, ):
		pass
	
	def auth(self, ):
		"""
		Set up secure control connection by using TLS or SSL, depending on what specified in :meth:`ssl_version` attribute.
		
		"""
		pass
		
	def prot_p(self, ):
		"""
		Set up secure data connection.
		
		"""
		pass
		
	"""
	The set of all exceptions (as a tuple) that methods of :class:`FTP`
	instances may raise as a result of problems with the FTP connection (as
	opposed to programming errors made by the caller).  This set includes the
	four exceptions listed above as well as :exc:`socket.error` and
	:exc:`IOError`.
	
	
	"""
	all_errors = None
	

