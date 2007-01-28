
/*
 * Copyright (c) 2007
 *      Shrew Soft Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Redistributions in any form must be accompanied by information on
 *    how to obtain complete source code for the software and any
 *    accompanying software that uses the software.  The source code
 *    must either be included in the distribution or be available for no
 *    more than the cost of distribution plus a nominal fee, and must be
 *    freely redistributable under reasonable conditions.  For an
 *    executable file, complete source code means the source code for all
 *    modules it contains.  It does not include source code for modules or
 *    files that typically accompany the major components of the operating
 *    system on which the executable file runs.
 *
 * THIS SOFTWARE IS PROVIDED BY SHREW SOFT INC ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR
 * NON-INFRINGEMENT, ARE DISCLAIMED.  IN NO EVENT SHALL SHREW SOFT INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * AUTHOR : Matthew Grooms
 *          mgrooms@shrew.net
 *
 */

#include "libiked.h"

#define	MAX_BASIC_MSG	sizeof( IKEI_MSG_BASIC ) + IKEI_MAX_BDATA

#ifdef WIN32

_IKEI::_IKEI()
{
	hpipe = INVALID_HANDLE_VALUE;
	memset( &olapp, 0, sizeof( olapp ) );
	wait = false;
}

_IKEI::~_IKEI()
{
	CloseHandle( olapp.hEvent );
}

long _IKEI::open( long timeout )
{
	if( !WaitNamedPipe( IKEI_PIPE_NAME, timeout ) )
		return IKEI_FAILED;

	hpipe = CreateFile(
				IKEI_PIPE_NAME,
				GENERIC_READ | GENERIC_WRITE,
				FILE_SHARE_READ | FILE_SHARE_WRITE,
				NULL,
				OPEN_EXISTING,
				FILE_FLAG_OVERLAPPED,
				NULL );

	if( hpipe == INVALID_HANDLE_VALUE )
		return IKEI_FAILED;

	return IKEI_OK;
}

void _IKEI::close()
{
	if( hpipe )
	{
		CloseHandle( hpipe );
		hpipe = INVALID_HANDLE_VALUE;
	}
}

void CALLBACK msg_end( DWORD result, DWORD size, LPOVERLAPPED overlapped )
{
}

long _IKEI::wait_msg( IKEI_MSG & msg, long timeout )
{
	if( !wait )
	{
		if( ReadFileEx( hpipe, &tmsg, sizeof( tmsg ), &olapp, &msg_end ) )
			wait = true;

		if( GetLastError() == ERROR_BROKEN_PIPE )
		{
			CloseHandle( hpipe );
			return IKEI_FAILED;
		}
	}

	if( !SleepEx( timeout, true ) )
		return IKEI_NODATA;

	wait = false;
	memcpy( &msg, &tmsg, sizeof( tmsg ) );

	return IKEI_OK;
}

long _IKEI::recv_msg( void * data, unsigned long & size, bool wait )
{
	//
	// read the rest of the message
	//

	memcpy( data, &tmsg, sizeof( tmsg ) );

	unsigned char * buff = ( unsigned char * ) data;

	long result = ReadFile( hpipe, buff + sizeof( tmsg ), tmsg.size - sizeof( tmsg ), &size, NULL );
	if( !result )
	{
		if( GetLastError() == ERROR_BROKEN_PIPE )
			return IKEI_FAILED;
	}

	return IKEI_OK;
}

long _IKEI::send_msg( void * data, unsigned long size )
{
	long result = WriteFile( hpipe, data, size, &size, &olapp );

	if( !result )
		if( GetLastError() == ERROR_BROKEN_PIPE )
			return IKEI_FAILED;

	return IKEI_OK;
}

#endif

#ifdef UNIX

_IKEI::_IKEI()
{
}

_IKEI::~_IKEI()
{
}

long _IKEI::open( long timeout )
{
	return IKEI_OK;
}

void _IKEI::close()
{
}

long _IKEI::wait_msg( IKEI_MSG & msg, long timeout )
{
	return IKEI_NODATA;
}

long _IKEI::recv_msg( void * data, unsigned long & size, bool wait )
{
	return IKEI_OK;
}

long _IKEI::send_msg( void * data, unsigned long size )
{
	return IKEI_OK;
}

#endif

long _IKEI::recv_basic( long type, long * value, void * bdata, long * bsize, bool wait )
{
	char msg_buff[ MAX_BASIC_MSG ];

	IKEI_MSG_BASIC *	msg_head = ( IKEI_MSG_BASIC * ) msg_buff;
	char *				msg_data = ( char * ) ( msg_buff + sizeof( IKEI_MSG_BASIC ) );
	unsigned long		msg_size = MAX_BASIC_MSG;

	long result;
	
	result = recv_msg( msg_buff, msg_size, wait );
	if( result != IKEI_OK )
		return result;

	assert( type == msg_head->msg.type );

	if( value )
		*value = msg_head->value;

	if( bdata && bsize )
	{
		if( *bsize < msg_head->bsize )
			return false;

		memcpy( bdata, msg_data, msg_head->bsize );

		*bsize = msg_head->bsize;
	}

	return IKEI_OK;
}

long _IKEI::send_basic( long type, long value, void * bdata, long bsize )
{
	char msg_buff[ MAX_BASIC_MSG ];

	IKEI_MSG_BASIC *	msg_head = ( IKEI_MSG_BASIC * ) msg_buff;
	char *				msg_data = ( char * ) ( msg_buff + sizeof( IKEI_MSG_BASIC ) );
	unsigned long		msg_size = sizeof( IKEI_MSG_BASIC ) + bsize;

	msg_head->msg.type = type;
	msg_head->msg.size = msg_size;
	msg_head->value = value;
	msg_head->bsize = bsize;

	memcpy( msg_data, bdata, bsize );

	return send_msg( msg_buff, msg_size );
}

long _IKEI::send_bidir( long type, long value, void * bdata, long bsize, long * msgres )
{
	long result;
	
	result = send_basic( type, value, bdata, bsize );
	if( result != IKEI_OK )
		return result;

	result = wait_msg( tmsg, 100000 );
	if( result != IKEI_OK )
		return result;

	return recv_basic( IKEI_MSGID_RESULT, msgres, NULL, NULL, true );
}

long _IKEI::next_msg( IKEI_MSG & msg )
{
	memset( &msg, 0, sizeof( msg ) );

	return wait_msg( msg, 10 );
}

//
// UNI-DIRECTION MESSAGE HANDLERS
//

long _IKEI::send_msg_result( long msgres )
{
	return send_basic( IKEI_MSGID_RESULT, msgres, NULL, 0 );
}

long _IKEI::recv_msg_status( long * status, char * str, long & len )
{
	return recv_basic( IKEI_MSGID_STATUS, status, str, &len );
}

long _IKEI::send_msg_status( long status, char * str, long * msgres )
{
	return send_basic( IKEI_MSGID_STATUS, status, str, strlen( str ) );
}

long _IKEI::recv_msg_stats( IPSEC_STATS * stats )
{
	long length = sizeof( IPSEC_STATS );
	return recv_basic( IKEI_MSGID_STATS, NULL, stats, &length ); 
}

long _IKEI::send_msg_stats( IPSEC_STATS * stats, long * msgres )
{
	return send_basic( IKEI_MSGID_STATS, 0, stats, sizeof( IPSEC_STATS ) );
}

long _IKEI::send_msg_enable( long enable )
{
	return send_basic( IKEI_MSGID_ENABLE, enable, NULL, 0 );
}

long _IKEI::recv_msg_enable( long * enable )
{
	return recv_basic( IKEI_MSGID_ENABLE, enable, NULL, NULL );
}

//
// BI-DIRECTION MESSAGE HANDLERS
//

long _IKEI::recv_msg_peer( IKE_PEER * peer )
{
	long length = sizeof( IKE_PEER );
	return recv_basic( IKEI_MSGID_PEER, NULL, peer, &length ); 
}

long _IKEI::send_msg_peer( IKE_PEER * peer, long * msgres )
{
	return send_bidir( IKEI_MSGID_PEER, 0, peer, sizeof( IKE_PEER ), msgres ); 
}

long _IKEI::recv_msg_proposal( IKE_PROPOSAL * proposal )
{
	long length = sizeof( IKE_PROPOSAL );
	return recv_basic( IKEI_MSGID_PROPOSAL, NULL, proposal, &length ); 
}

long _IKEI::send_msg_proposal( IKE_PROPOSAL * proposal, long * msgres )
{
	return send_bidir( IKEI_MSGID_PROPOSAL, 0, proposal, sizeof( IKE_PROPOSAL ), msgres ); 
}

long _IKEI::recv_msg_client( IKE_XCONF * xconf )
{
	long length = sizeof( IKE_XCONF );
	return recv_basic( IKEI_MSGID_CLIENT, NULL, xconf, &length ); 
}

long _IKEI::send_msg_client( IKE_XCONF * xconf, long * msgres )
{
	return send_bidir( IKEI_MSGID_CLIENT, 0, xconf, sizeof( IKE_XCONF ), msgres );
}

long _IKEI::recv_msg_network( IKE_PH2ID * ph2id, long * type )
{
	long length = sizeof( IKE_PH2ID );
	return recv_basic( IKEI_MSGID_NETWORK, type, ph2id, &length ); 
}

long _IKEI::recv_msg_network( IKE_PH2ID * ph2id, long type, long * msgres )
{
	return send_bidir( IKEI_MSGID_NETWORK, type, ph2id, sizeof( IKE_PH2ID ), msgres );
}

long _IKEI::recv_msg_cfgstr( long * type, char * str, long * len )
{
	return recv_basic( IKEI_MSGID_CFGSTR, type, str, len );
}

long _IKEI::send_msg_cfgstr( long type, char * str, long len, long * msgres )
{
	return send_bidir( IKEI_MSGID_CFGSTR, type, str, len, msgres );
}

#ifdef UNIX

_IKES::_IKES()
{
}

_IKES::~_IKES()
{
}

bool _IKES::init()
{
	return true;
}

IKEI * _IKES::accept()
{
	return NULL;
}

#endif

#ifdef WIN32

_IKES::_IKES()
{
	hsrvc = INVALID_HANDLE_VALUE;
	hpipe = INVALID_HANDLE_VALUE;
}

_IKES::~_IKES()
{
}

//
// check if a named pipe already exists
//

bool _IKES::init()
{
	hsrvc = OpenEvent(
				EVENT_ALL_ACCESS,
				false,
				IKEI_EVENT_NAME );

	if( hsrvc )
	{
		CloseHandle( hsrvc );
		return false;
	}

	hsrvc =	CreateEvent(
				NULL,
				false,
				false,
				IKEI_EVENT_NAME );

	if( hsrvc == INVALID_HANDLE_VALUE )
		return false;

	memset( &olapp, 0, sizeof( olapp ) );
	olapp.hEvent = CreateEvent( NULL, true, false, NULL );

	return true;
}

//
// accept connections on the named pipe
//

IKEI * _IKES::accept()
{
	if( hpipe == INVALID_HANDLE_VALUE )
	{
		PSID sid = NULL;
		PACL acl = NULL;
		EXPLICIT_ACCESS ea;
		SID_IDENTIFIER_AUTHORITY sid_auth = SECURITY_WORLD_SID_AUTHORITY;
		PSECURITY_DESCRIPTOR sd = NULL;
		SECURITY_ATTRIBUTES sa;
		long result;

		// create a well-known sid
		if( !AllocateAndInitializeSid(
				&sid_auth,
				1,
				SECURITY_WORLD_RID,
				0,
				0, 0, 0, 0, 0, 0,
				&sid ) )
			goto cleanup;

		// Initialize an EXPLICIT_ACCESS structure for an ACE.
		// The ACE will allow Everyone read access to the key.
		memset( &ea, sizeof( ea ), 0 );
		ea.grfAccessPermissions = KEY_READ;
		ea.grfAccessMode = SET_ACCESS;
		ea.grfInheritance= NO_INHERITANCE;
		ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
		ea.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
		ea.Trustee.ptstrName  = ( LPTSTR ) sid;

		// Initialize a security descriptor.  
		sd = ( PSECURITY_DESCRIPTOR ) LocalAlloc( LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH );
		if( sd == NULL )
			goto cleanup; 
 
		if( !InitializeSecurityDescriptor( sd, SECURITY_DESCRIPTOR_REVISION ) ) 
			goto cleanup; 
 
		// Add the ACL to the security descriptor. 
		if( !SetSecurityDescriptorDacl(
				sd, 
				TRUE,     // bDaclPresent flag   
				acl, 
				FALSE ) )   // not a default DACL 
			goto cleanup; 

		// Initialize a security attributes structure.
		sa.nLength = sizeof ( SECURITY_ATTRIBUTES );
		sa.lpSecurityDescriptor = sd;
		sa.bInheritHandle = FALSE;

		hpipe = CreateNamedPipe(
				IKEI_PIPE_NAME,
				PIPE_ACCESS_DUPLEX |
				FILE_FLAG_OVERLAPPED,
			    PIPE_TYPE_MESSAGE |
				PIPE_READMODE_MESSAGE |
				PIPE_NOWAIT,
				PIPE_UNLIMITED_INSTANCES,
				8192,
				8192,
			    10,
				&sa );

		result = GetLastError();

		cleanup:

		if( sid != NULL )
			FreeSid( sid );
		if( acl )
			LocalFree( acl );
		if( sd )
			LocalFree( sd );
	}

	if( ( ConnectNamedPipe( hpipe, NULL ) == TRUE ) ||
		( GetLastError() == ERROR_PIPE_CONNECTED ) )
	{
		IKEI * ikedi = new IKEI;
		if( ikedi == NULL )
			return NULL;

		ikedi->hpipe = hpipe;
		hpipe = INVALID_HANDLE_VALUE;

		return ikedi;
	}

	return NULL;
}

#endif
