/*
 * OpenBTS provides an open source alternative to legacy telco protocols and 
 * traditionally complex, proprietary hardware systems.
 *
 * Copyright 2008, 2010 Free Software Foundation, Inc.
 * Copyright 2011-2014 Range Networks, Inc.
 *
 * This software is distributed under the terms of the GNU Affero General 
 * Public License version 3. See the COPYING and NOTICE files in the main 
 * directory for licensing information.
 *
 * This use of this software may be subject to additional restrictions.
 * See the LEGAL file in the main directory for details.
 */


#ifndef SOCKETS_H
#define SOCKETS_H

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <errno.h>
#include <list>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>





#define MAX_UDP_LENGTH 8000

/** A function to resolve IP host names. */
bool resolveAddress(struct sockaddr_in *address, const char *host, unsigned short port);

/** Resolve an address of the form "<host>:<port>". */
bool resolveAddress(struct sockaddr_in *address, const char *hostAndPort);

/** An exception to throw when a critical socket operation fails. */
class SocketError {};
#define SOCKET_ERROR {throw SocketError(); }

/** Abstract class for connectionless sockets. */
class DatagramSocket {

protected:

	int mSocketFD;				///< underlying file descriptor
	char mDestination[256];		///< address to which packets are sent
	char mSource[256];		///< return address of most recent received packet

public:

	/** An almost-does-nothing constructor. */
	DatagramSocket();

	virtual ~DatagramSocket();

	/** Return the address structure size for this socket type. */
	virtual size_t addressSize() const = 0;

	/**
		Send a binary packet.
		@param buffer The data bytes to send to mDestination.
		@param length Number of bytes to send, or strlen(buffer) if defaulted to -1.
		@return number of bytes written, or -1 on error.
	*/
	int write( const char * buffer, size_t length);

	/**
		Send a C-style string packet.
		@param buffer The data bytes to send to mDestination.
		@return number of bytes written, or -1 on error.
	*/
	int write( const char * buffer);

	/**
		Send a binary packet.
		@param buffer The data bytes to send to mSource.
		@param length Number of bytes to send, or strlen(buffer) if defaulted to -1.
		@return number of bytes written, or -1 on error.
	*/
	int writeBack(const char * buffer, size_t length);

	/**
		Send a C-style string packet.
		@param buffer The data bytes to send to mSource.
		@return number of bytes written, or -1 on error.
	*/
	int writeBack(const char * buffer);


	/**
		Receive a packet.
		@param buffer A char[MAX_UDP_LENGTH] procured by the caller.
		@return The number of bytes received or -1 on non-blocking pass.
	*/
	int read(char* buffer);

	/**
		Receive a packet with a timeout.
		@param buffer A char[MAX_UDP_LENGTH] procured by the caller.
		@param maximum wait time in milliseconds
		@return The number of bytes received or -1 on timeout.
	*/
	int read(char* buffer, unsigned timeout);


	/** Send a packet to a given destination, other than the default. */
	int send(const struct sockaddr *dest, const char * buffer, size_t length);

	/** Send a C-style string to a given destination, other than the default. */
	int send(const struct sockaddr *dest, const char * buffer);

	/** Make the socket non-blocking. */
	void nonblocking();

	/** Make the socket blocking (the default). */
	void blocking();

	/** Close the socket. */
	void close();

};



/** UDP/IP User Datagram Socket */
class UDPSocket : public DatagramSocket {

public:

	/** Open a USP socket with an OS-assigned port and no default destination. */
	UDPSocket( unsigned short localPort=0);

	/** Given a full specification, open the socket and set the dest address. */
	UDPSocket( 	unsigned short localPort, 
			const char * remoteIP, unsigned short remotePort);

	/** Set the destination port. */
	void destination( unsigned short wDestPort, const char * wDestIP );

	/** Return the actual port number in use. */
	unsigned short port() const;

	/** Open and bind the UDP socket to a local port. */
	void open(unsigned short localPort=0);

	/** Give the return address of the most recently received packet. */
	const struct sockaddr_in* source() const { return (const struct sockaddr_in*)mSource; }

	size_t addressSize() const { return sizeof(struct sockaddr_in); }

};


/** Unix Domain Datagram Socket */
class UDDSocket : public DatagramSocket {

public:

	UDDSocket(const char* localPath=NULL, const char* remotePath=NULL);

	void destination(const char* remotePath);

	void open(const char* localPath);

	/** Give the return address of the most recently received packet. */
	const struct sockaddr_un* source() const { return (const struct sockaddr_un*)mSource; }

	size_t addressSize() const { return sizeof(struct sockaddr_un); }

};


#endif



// vim:ts=4:sw=4
