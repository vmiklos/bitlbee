  /********************************************************************\
  * BitlBee -- An IRC to other IM-networks gateway                     *
  *                                                                    *
  * Copyright 2002-2006 Wilmer van der Gaast and others                *
  \********************************************************************/

/* Storage backend that uses an XMLish format for all data. */

/*
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License with
  the Debian GNU/Linux distribution in /usr/share/common-licenses/GPL;
  if not, write to the Free Software Foundation, Inc., 59 Temple Place,
  Suite 330, Boston, MA  02111-1307  USA
*/

#define BITLBEE_CORE
#include "bitlbee.h"
#include "md5.h"

typedef enum
{
	XML_PASS_CHECK_ONLY = -1,
	XML_PASS_UNKNOWN = 0,
	XML_PASS_OK
} xml_pass_st;

/* This isn't very clean, probably making a separate error class + code for
   BitlBee would be a better solution. But this will work for now... */
#define XML_PASS_ERRORMSG "Wrong username or password"

struct xml_parsedata
{
	irc_t *irc;
	char *current_setting;
	account_t *current_account;
	char *given_nick;
	char *given_pass;
	xml_pass_st pass_st;
};

static char *xml_attr( const gchar **attr_names, const gchar **attr_values, const gchar *key )
{
	int i;
	
	for( i = 0; attr_names[i]; i ++ )
		if( g_strcasecmp( attr_names[i], key ) == 0 )
			return (char*) attr_values[i];
	
	return NULL;
}

static void xml_destroy_xd( gpointer data )
{
	struct xml_parsedata *xd = data;
	
	g_free( xd->given_nick );
	g_free( xd->given_pass );
	g_free( xd );
}

static void xml_start_element( GMarkupParseContext *ctx, const gchar *element_name, const gchar **attr_names, const gchar **attr_values, gpointer data, GError **error )
{
	struct xml_parsedata *xd = data;
	irc_t *irc = xd->irc;
	
	if( g_strcasecmp( element_name, "user" ) == 0 )
	{
		char *nick = xml_attr( attr_names, attr_values, "nick" );
		char *pass = xml_attr( attr_names, attr_values, "password" );
		
		if( !nick || !pass )
		{
			g_set_error( error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
			             "Missing attributes for %s element", element_name );
		}
		else
		{
			md5_byte_t pass_md5[16];
			md5_state_t md5_state;
			int i, j;
			
			md5_init( &md5_state );
			md5_append( &md5_state, (md5_byte_t*) xd->given_pass, strlen( xd->given_pass ) );
			md5_finish( &md5_state, pass_md5 );
			
			for( i = 0; i < 16; i ++ )
			{
				if( !isxdigit( pass[i*2] ) || !isxdigit( pass[i*2+1] ) ||
				     sscanf( pass + i * 2, "%2x", &j ) != 1 )
				{
					g_set_error( error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
					             "Incorrect password MD5-hash" );
					break;
				}
				if( j != pass_md5[i] )
				{
					g_set_error( error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
					             XML_PASS_ERRORMSG );
					break;
				}
			}
			
			/* If we reached the end of the loop, it was a match! */
			if( i == 16 )
			{
				if( xd->pass_st != XML_PASS_CHECK_ONLY )
					xd->pass_st = XML_PASS_OK;
			}
		}
	}
	else if( xd->pass_st < XML_PASS_OK )
	{
		/* Let's not parse anything else if we only have to check
		   the password. */
	}
	else if( g_strcasecmp( element_name, "account" ) == 0 )
	{
		char *protocol, *handle, *server, *password;
		struct prpl *prpl = NULL;
		
		handle = xml_attr( attr_names, attr_values, "handle" );
		password = xml_attr( attr_names, attr_values, "password" );
		server = xml_attr( attr_names, attr_values, "server" );
		
		protocol = xml_attr( attr_names, attr_values, "protocol" );
		if( protocol )
			prpl = find_protocol( protocol );
		
		if( !handle || !password )
			g_set_error( error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
			             "Missing attributes for %s element", element_name );
		else if( !prpl )
			g_set_error( error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
			             "Missing or unknown protocol %s element", element_name );
		else
		{
			xd->current_account = account_add( irc, prpl, handle, password );
			if( server )
				xd->current_account->server = g_strdup( server );
		}
	}
	else if( g_strcasecmp( element_name, "setting" ) == 0 )
	{
		if( xd->current_account == NULL )
		{
			char *setting;
			
			if( xd->current_setting )
			{
				g_free( xd->current_setting );
				xd->current_setting = NULL;
			}
			
			if( ( setting = xml_attr( attr_names, attr_values, "name" ) ) )
				xd->current_setting = g_strdup( setting );
			else
				g_set_error( error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
				             "Missing attributes for %s element", element_name );
		}
	}
	else if( g_strcasecmp( element_name, "buddy" ) == 0 )
	{
		char *handle, *nick;
		
		handle = xml_attr( attr_names, attr_values, "handle" );
		nick = xml_attr( attr_names, attr_values, "nick" );
		
		if( xd->current_account && handle && nick )
		{
			nick_set( irc, handle, xd->current_account->prpl, nick );
		}
		else
		{
			g_set_error( error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
			             "Missing attributes for %s element", element_name );
		}
	}
	else
	{
		g_set_error( error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
		             "Unkown element: %s", element_name );
	}
}

static void xml_end_element( GMarkupParseContext *ctx, const gchar *element_name, gpointer data, GError **error )
{
	struct xml_parsedata *xd = data;
	
	if( g_strcasecmp( element_name, "setting" ) == 0 && xd->current_setting )
	{
		g_free( xd->current_setting );
		xd->current_setting = NULL;
	}
	else if( g_strcasecmp( element_name, "account" ) == 0 )
	{
		xd->current_account = NULL;
	}
}

static void xml_text( GMarkupParseContext *ctx, const gchar *text, gsize text_len, gpointer data, GError **error )
{
	struct xml_parsedata *xd = data;
	irc_t *irc = xd->irc;
	
	if( xd->pass_st < XML_PASS_OK )
	{
		/* Let's not parse anything else if we only have to check
		   the password, or if we didn't get the chance to check it
		   yet. */
	}
	else if( g_strcasecmp( g_markup_parse_context_get_element( ctx ), "setting" ) == 0 &&
	         xd->current_setting && xd->current_account == NULL )
	{
		set_setstr( irc, xd->current_setting, (char*) text );
		g_free( xd->current_setting );
		xd->current_setting = NULL;
	}
}

GMarkupParser xml_parser =
{
	xml_start_element,
	xml_end_element,
	xml_text,
	NULL,
	NULL
};

static void xml_init( void )
{
	if( access( global.conf->configdir, F_OK ) != 0 )
		log_message( LOGLVL_WARNING, "The configuration directory %s does not exist. Configuration won't be saved.", CONFIG );
	else if( access( global.conf->configdir, R_OK ) != 0 || access( global.conf->configdir, W_OK ) != 0 )
		log_message( LOGLVL_WARNING, "Permission problem: Can't read/write from/to %s.", global.conf->configdir );
}

static storage_status_t xml_load_real( const char *my_nick, const char *password, irc_t *irc, xml_pass_st action )
{
	GMarkupParseContext *ctx;
	struct xml_parsedata *xd;
	char *fn, buf[512];
	GError *gerr = NULL;
	int fd, st;
	
	if( irc && irc->status & USTATUS_IDENTIFIED )
		return( 1 );
	
	xd = g_new0( struct xml_parsedata, 1 );
	xd->irc = irc;
	xd->given_nick = g_strdup( my_nick );
	xd->given_pass = g_strdup( password );
	xd->pass_st = action;
	nick_lc( xd->given_nick );
	
	fn = g_strdup_printf( "%s%s%s", global.conf->configdir, xd->given_nick, ".xml" );
	if( ( fd = open( fn, O_RDONLY ) ) < 0 )
	{
		xml_destroy_xd( xd );
		g_free( fn );
		return STORAGE_NO_SUCH_USER;
	}
	g_free( fn );
	
	ctx = g_markup_parse_context_new( &xml_parser, 0, xd, xml_destroy_xd );
	
	while( ( st = read( fd, buf, sizeof( buf ) ) ) > 0 )
	{
		if( !g_markup_parse_context_parse( ctx, buf, st, &gerr ) || gerr )
		{
			g_markup_parse_context_free( ctx );
			close( fd );
			
			/* Slightly dirty... */
			if( gerr && strcmp( gerr->message, XML_PASS_ERRORMSG ) == 0 )
				return STORAGE_INVALID_PASSWORD;
			else
			{
				if( gerr && irc )
					irc_usermsg( irc, "Error from XML-parser: %s", gerr->message );
				
				return STORAGE_OTHER_ERROR;
			}
		}
	}
	
	g_markup_parse_context_free( ctx );
	close( fd );
	
	if( action == XML_PASS_CHECK_ONLY )
		return STORAGE_OK;
	
	irc->status |= USTATUS_IDENTIFIED;
	
	if( set_getint( irc, "auto_connect" ) )
	{
		/* Can't do this directly because r_c_s alters the string */
		strcpy( buf, "account on" );
		root_command_string( irc, NULL, buf, 0 );
	}
	
	return STORAGE_OK;
}

static storage_status_t xml_load( const char *my_nick, const char *password, irc_t *irc )
{
	return xml_load_real( my_nick, password, irc, XML_PASS_UNKNOWN );
}

static storage_status_t xml_check_pass( const char *my_nick, const char *password )
{
	/* This is a little bit risky because we have to pass NULL for the
	   irc_t argument. This *should* be fine, if I didn't miss anything... */
	return xml_load_real( my_nick, password, NULL, XML_PASS_CHECK_ONLY );
}

static int xml_printf( int fd, char *fmt, ... )
{
	va_list params;
	char *out;
	int len;
	
	va_start( params, fmt );
	out = g_markup_vprintf_escaped( fmt, params );
	va_end( params );
	
	len = strlen( out );
	len -= write( fd, out, len );
	g_free( out );
	
	return len == 0;
}

static storage_status_t xml_save( irc_t *irc, int overwrite )
{
	char path[512], *path2, md5_buf[33];
	set_t *set;
	nick_t *nick;
	account_t *acc;
	int fd, i;
	md5_byte_t pass_md5[16];
	md5_state_t md5_state;
	
	if( irc->password == NULL )
	{
		irc_usermsg( irc, "Please register yourself if you want to save your settings." );
		return STORAGE_OTHER_ERROR;
	}
	
	g_snprintf( path, sizeof( path ) - 2, "%s%s%s", global.conf->configdir, irc->nick, ".xml" );
	
	if( !overwrite && access( path, F_OK ) != -1 )
		return STORAGE_ALREADY_EXISTS;
	
	strcat( path, "~" );
	if( ( fd = open( path, O_WRONLY | O_CREAT, 0600 ) ) < 0 )
	{
		irc_usermsg( irc, "Error while opening configuration file." );
		return STORAGE_OTHER_ERROR;
	}
	
	md5_init( &md5_state );
	md5_append( &md5_state, (md5_byte_t*) irc->password, strlen( irc->password ) );
	md5_finish( &md5_state, pass_md5 );
	for( i = 0; i < 16; i ++ )
		g_snprintf( md5_buf + i * 2, 3, "%02x", pass_md5[i] );
	
	if( !xml_printf( fd, "<user nick=\"%s\" password=\"%s\">\n", irc->nick, md5_buf ) )
		goto write_error;
	
	for( set = irc->set; set; set = set->next )
		if( set->value && set->def )
			if( !xml_printf( fd, "\t<setting name=\"%s\">%s</setting>\n", set->key, set->value ) )
				goto write_error;
	
	for( acc = irc->accounts; acc; acc = acc->next )
	{
		if( !xml_printf( fd, "\t<account protocol=\"%s\" handle=\"%s\" password=\"%s\" autoconnect=\"%s\"", acc->prpl->name, acc->user, acc->pass, "yes" ) )
			goto write_error;
		if( acc->server && acc->server[0] && !xml_printf( fd, " server=\"%s\"", acc->server ) )
			goto write_error;
		if( !xml_printf( fd, ">\n" ) )
			goto write_error;
		
		for( nick = irc->nicks; nick; nick = nick->next )
			if( nick->proto == acc->prpl )
				if( !xml_printf( fd, "\t\t<buddy handle=\"%s\" nick=\"%s\" />\n", nick->handle, nick->nick ) )
					goto write_error;
		
		if( !xml_printf( fd, "\t</account>\n" ) )
			goto write_error;
	}
	
	if( !xml_printf( fd, "</user>\n" ) )
		goto write_error;
	
	close( fd );
	
	path2 = g_strndup( path, strlen( path ) - 1 );
	if( rename( path, path2 ) != 0 )
	{
		irc_usermsg( irc, "Error while renaming temporary configuration file." );
		
		g_free( path2 );
		unlink( path );
		
		return STORAGE_OTHER_ERROR;
	}
	
	g_free( path2 );
	
	return STORAGE_OK;

write_error:
	irc_usermsg( irc, "Write error. Disk full?" );
	close( fd );
	
	return STORAGE_OTHER_ERROR;
}

static storage_status_t xml_remove( const char *nick, const char *password )
{
	char s[512];
	storage_status_t status;

	status = xml_check_pass( nick, password );
	if( status != STORAGE_OK )
		return status;

	g_snprintf( s, 511, "%s%s%s", global.conf->configdir, nick, ".xml" );
	if( unlink( s ) == -1 )
		return STORAGE_OTHER_ERROR;
	
	return STORAGE_OK;
}

storage_t storage_xml = {
	.name = "xml",
	.init = xml_init,
	.check_pass = xml_check_pass,
	.remove = xml_remove,
	.load = xml_load,
	.save = xml_save
};
