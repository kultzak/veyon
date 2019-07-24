/*
 * AuthLogonPlugin.cpp - implementation of AuthLogonPlugin class
 *
 * Copyright (c) 2018-2019 Tobias Junghans <tobydox@veyon.io>
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

#include <QApplication>

#include "AuthLogonPlugin.h"
#include "Filesystem.h"
#include "PasswordDialog.h"
#include "PlatformUserFunctions.h"
#include "VariantArrayMessage.h"
#include "VeyonConfiguration.h"

AuthLogonPlugin::AuthLogonPlugin( QObject* parent ) :
	QObject( parent )
{
}



bool AuthLogonPlugin::initializeCredentials()
{
	if( qobject_cast<QApplication *>( QCoreApplication::instance() ) )
	{
		PasswordDialog passwordDialog( QApplication::activeWindow() );
		if( passwordDialog.exec() == PasswordDialog::Accepted )
		{
			m_username = passwordDialog.username();
			m_password = passwordDialog.password();

			return true;
		}
	}

	return false;
}



bool AuthLogonPlugin::hasCredentials() const
{
	return m_username.isEmpty() == false && m_password.isEmpty() == false;
}



bool AuthLogonPlugin::testConfiguration() const
{
	return PasswordDialog( QApplication::activeWindow() ).exec() == PasswordDialog::Accepted;
}



VncServerClient::AuthState AuthLogonPlugin::performAuthentication( VncServerClient* client, VariantArrayMessage& message ) const
{
	switch( client->authState() )
	{
	case VncServerClient::AuthState::Init:
		client->setPrivateKey( CryptoCore::KeyGenerator().createRSA( CryptoCore::RsaKeySize ) );

		if( VariantArrayMessage( message.ioDevice() ).write( client->privateKey().toPublicKey().toPEM() ).send() )
		{
			return VncServerClient::AuthState::Stage1;
		}

		vDebug() << "failed to send public key";
		return VncServerClient::AuthState::Failed;

	case VncServerClient::AuthState::Stage1:
	{
		auto privateKey = client->privateKey();

		client->setUsername( message.read().toString() ); // Flawfinder: ignore
		CryptoCore::SecureArray encryptedPassword( message.read().toByteArray() ); // Flawfinder: ignore

		CryptoCore::SecureArray decryptedPassword;

		if( privateKey.decrypt( encryptedPassword,
								&decryptedPassword,
								CryptoCore::DefaultEncryptionAlgorithm ) == false )
		{
			vWarning() << "failed to decrypt password";
			return VncServerClient::AuthState::Failed;
		}

		vInfo() << "authenticating user" << client->username();

		if( VeyonCore::platform().userFunctions().authenticate( client->username(), decryptedPassword.toByteArray() ) )
		{
			vDebug() << "SUCCESS";
			return VncServerClient::AuthState::Successful;
		}

		vDebug() << "FAIL";
		return VncServerClient::AuthState::Failed;
	}

	default:
		break;
	}

	return VncServerClient::AuthState::Failed;

}



bool AuthLogonPlugin::authenticate( QIODevice* socket ) const
{
	VariantArrayMessage publicKeyMessage( socket );
	publicKeyMessage.receive();

	CryptoCore::PublicKey publicKey = CryptoCore::PublicKey::fromPEM( publicKeyMessage.read().toString() );

	if( publicKey.canEncrypt() == false )
	{
		vCritical() << QThread::currentThreadId() << "can't encrypt with given public key!";
		return false;
	}

	CryptoCore::SecureArray encryptedPassword = publicKey.encrypt( m_password, CryptoCore::DefaultEncryptionAlgorithm );
	if( encryptedPassword.isEmpty() )
	{
		vCritical() << QThread::currentThreadId() << "password encryption failed!";
		return false;
	}

	VariantArrayMessage response( socket );
	response.write( m_username );
	response.write( encryptedPassword.toByteArray() );
	response.send();

	return true;
}
