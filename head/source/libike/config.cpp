
/*
 * Copyright (c) 2007
 *      Shrew Soft Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, is strictly prohibited. The copywright holder of this
 * software is the sole owner and no other party should have access
 * unless explicit permission was granted by an authorized person.
 *
 * AUTHOR : Matthew Grooms
 *          mgrooms@shrew.net
 *
 */

#include "config.h"

#define DELIM_NEW	','
#define DELIM_OLD	0x255

inline char * text_delim( char * text )
{
	char * delim;

	delim = strchr( text, DELIM_NEW );
	if( delim == NULL )
		delim = strchr( text, DELIM_OLD );

	return delim;
}

inline size_t text_length( char * text )
{
	size_t oset = 0;

	while( true )
	{
		int c = text[ oset ];

		switch( c )
		{
			case 0:
			case DELIM_OLD:
			case DELIM_NEW:
				return oset;

			default:
				oset++;
		}
	}

	return 0;
}

/*
 * CONFIG class member functions
 */

_CFGDAT::_CFGDAT()
{
	nval = 0;
}

_CONFIG::_CONFIG()
{
}

_CONFIG::~_CONFIG()
{
}

bool _CONFIG::set_id( char * set_id )
{
	id.del();
	id.set( set_id, strlen( set_id ) + 1 );
	return true;
}

char * _CONFIG::get_id()
{
	return id.text();
}

_CONFIG & _CONFIG::operator = ( _CONFIG & config )
{
	del_all();
	set_id( config.get_id() );

	for( long index = 0; index < config.count(); index++ )
	{
		CFGDAT * cfgdat = static_cast<CFGDAT*>( config.get_entry( index ) );
		switch( cfgdat->type )
		{
			case DATA_STRING:
				set_string( cfgdat->key.text(), cfgdat->vval.text(), cfgdat->vval.size() );
				break;

			case DATA_NUMBER:
				set_number( cfgdat->key.text(), cfgdat->nval );
				break;

			case DATA_BINARY:
				set_binary( cfgdat->key.text(), cfgdat->vval );
				break;
		}
	}
	
	return *this;
}

CFGDAT * _CONFIG::get_data( long type, char * key, bool add )
{
	CFGDAT * cfgdat;

	for( long index = 0; index < count(); index++ )
	{
		cfgdat = static_cast<CFGDAT*>( get_entry( index ) );

		if( cfgdat->type != type )
			continue;
		
		if( !_stricmp( cfgdat->key.text(), key ) )
			return cfgdat;
	}

	if( add )
	{
		cfgdat = new CFGDAT;
		if( cfgdat == NULL )
			return NULL;

		cfgdat->type = type;
		cfgdat->key.set( key, strlen( key ) + 1 );
		add_entry( cfgdat );

		return cfgdat;
	}

	return NULL;
}

void _CONFIG::del( char * key )
{
	CFGDAT * cfgdat;

	for( long index = 0; index < count(); index++ )
	{
		cfgdat = static_cast<CFGDAT*>( get_entry( index ) );

		if( !_stricmp( cfgdat->key.text(), key ) )
		{
			del_entry( cfgdat );
			delete cfgdat;
		}
	}
}

void _CONFIG::del_all()
{
	clean();
}

bool _CONFIG::add_string( char * key, char * val, size_t size )
{
	CFGDAT * cfgdat = get_data( DATA_STRING, key, true );
	if( !cfgdat )
		return false;

	if( cfgdat->vval.size() )
	{
		cfgdat->vval.set( ",", 1, cfgdat->vval.size() - 1 );
		cfgdat->vval.add( val, size );
		cfgdat->vval.add( "", 1 );
	}
	else
	{
		cfgdat->vval.add( val, size );
		cfgdat->vval.add( "", 1 );
	}

	return true;
}

bool _CONFIG::set_string( char * key, char * val, size_t size )
{
	del( key );
	add_string( key, val, size );

	return true;
}

bool _CONFIG::get_string( char * key, char * val, size_t size, int index )
{
	CFGDAT * cfgdat = get_data( DATA_STRING, key );
	if( !cfgdat )
		return false;

	char * strptr = cfgdat->vval.text();

	for( ; index > 0; index-- )
	{
		char * tmpptr = text_delim( strptr );
		if( tmpptr == NULL )
			return false;

		strptr = tmpptr + 1;
	}

	// calculate final length

	size--;

	size_t clen = text_length( strptr );
	if( clen < size )
		size = clen;

	memcpy( val, strptr, size );
	val[ size ] = 0;

	return true;
}

long _CONFIG::has_string( char * key, char * val, size_t size )
{
	CFGDAT * cfgdat = get_data( DATA_STRING, key );
	if( !cfgdat )
		return -1;

	char * oldptr = cfgdat->vval.text();
	char * newptr = cfgdat->vval.text();

	long index = 0;

	while( newptr )
	{
		newptr = text_delim( oldptr );

		if( newptr )
		{
			size_t diff = newptr - oldptr;
			if( diff < size )
				size = diff;
		}

		if( !strncmp( val, oldptr, size ) )
			return index;
		
		oldptr = newptr + 1;
		index++;
	}

	return -1;
}

bool _CONFIG::set_number( char * key, long val )
{
	CFGDAT * cfgdat = get_data( DATA_NUMBER, key, true );
	if( !cfgdat )
		return false;

	cfgdat->nval = val;

	return true;
}

bool _CONFIG::get_number( char * key, long * val )
{
	CFGDAT * cfgdat = get_data( DATA_NUMBER, key );
	if( !cfgdat )
		return false;

	*val = cfgdat->nval;

	return true;
}

bool _CONFIG::set_binary( char * key, BDATA & val )
{
	CFGDAT * cfgdat = get_data( DATA_BINARY, key, true );
	if( !cfgdat )
		return false;

	cfgdat->vval = val;

	return true;
}

bool _CONFIG::get_binary( char * key, BDATA & val )
{
	CFGDAT * cfgdat = get_data( DATA_BINARY, key );
	if( !cfgdat )
		return false;

	val = cfgdat->vval;

	return true;
}

bool config_cmp_number( CONFIG * config_old, CONFIG * config_new, char * key )
{
	if( !config_old )
		return false;

	long number1;
	long number2;

	if( config_old->get_number( key, &number1 ) &&
		config_new->get_number( key, &number2 ) )
		if( number1 != number2 )
			return false;

	return true;
}

bool config_cmp_string( CONFIG * config_old, CONFIG * config_new, char * key )
{
	if( !config_old )
		return false;

	char string1[ MAX_CONFSTRING + 1 ];
	char string2[ MAX_CONFSTRING + 1 ];

	if( config_old->get_string( key, string1, MAX_CONFSTRING, 0 ) &&
		config_new->get_string( key, string2, MAX_CONFSTRING, 0 ) )
		if( strcmp( string1, string2 ) )
			return false;

	return true;
}
