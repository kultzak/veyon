/*
 * ServerAuthenticationManager.cpp - implementation of ServerAuthenticationManager
 *
 * Copyright (c) 2017-2019 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program (see COPYING); if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "AuthenticationManager.h"
#include "ServerAuthenticationManager.h"


ServerAuthenticationManager::ServerAuthenticationManager( QObject* parent ) :
	QObject( parent )
{
}



VncServerProtocol::AuthPluginUids ServerAuthenticationManager::supportedAuthPluginUids() const
{
	return VncServerProtocol::AuthPluginUids::fromList( VeyonCore::authenticationManager().plugins().keys() );
}



void ServerAuthenticationManager::processAuthenticationMessage( VncServerClient* client,
																VariantArrayMessage& message )
{
	vDebug() << "state" << client->authState()
			 << "plugin" << client->authPluginUid()
			 << "host" << client->hostAddress()
			 << "user" << client->username();

	const auto plugins = VeyonCore::authenticationManager().plugins();
	if( plugins.contains( client->authPluginUid() ) )
	{
		client->setAuthState( plugins[client->authPluginUid()]->performAuthentication( client, message ) );
	}
	else
	{
		client->setAuthState( VncServerClient::AuthState::Failed );
	}

	switch( client->authState() )
	{
	case VncServerClient::AuthState::Failed:
	case VncServerClient::AuthState::Successful:
		emit finished( client );
		break;
	default:
		break;
	}
}
