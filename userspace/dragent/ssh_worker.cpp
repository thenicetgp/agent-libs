#include "ssh_worker.h"

#include <sys/types.h>
#include <sys/wait.h>

const string ssh_worker::m_name = "ssh_worker";

Mutex ssh_worker::m_pending_messages_lock;
map<string, ssh_worker::pending_message> ssh_worker::m_pending_messages;

ssh_worker::ssh_worker(dragent_configuration* configuration, protocol_queue* queue,
		const string& token, const ssh_settings& settings):
	
	m_configuration(configuration),
	m_queue(queue),
	m_token(token),
	m_ssh_settings(settings),
	m_last_activity_ns(0),
	m_libssh_session(NULL),
	m_libssh_key(NULL),
	m_libssh_channel(NULL)
{
}

ssh_worker::~ssh_worker()
{
	delete_pending_messages(m_token);

	if(m_libssh_channel)
	{
		ssh_channel_close(m_libssh_channel);
		ssh_channel_free(m_libssh_channel);
		m_libssh_channel = NULL;
	}

	if(m_libssh_session)
	{
		ssh_disconnect(m_libssh_session);
		ssh_free(m_libssh_session);
		m_libssh_session = NULL;
	}

	if(m_libssh_key)
	{
		ssh_key_free(m_libssh_key);
		m_libssh_key = NULL;
	}
}

void ssh_worker::run()
{
	//
	// A quick hack to automatically delete this object
	//
	SharedPtr<ssh_worker> ptr(this);

	add_pending_messages(m_token);
	
	g_log->information(m_name + ": Opening SSH session, token " + m_token);

	m_libssh_session = ssh_new();
	if(m_libssh_session == NULL)
	{
		send_error("Error creating libssh session");
		return;
	}

	ssh_options_set(m_libssh_session, SSH_OPTIONS_HOST, "localhost");
	ssh_options_set(m_libssh_session, SSH_OPTIONS_USER, m_ssh_settings.m_user.c_str());

	if(m_ssh_settings.m_port)
	{
		ssh_options_set(m_libssh_session, SSH_OPTIONS_PORT, &m_ssh_settings.m_port);
	}

	if(ssh_connect(m_libssh_session) != SSH_OK)
	{
		send_error(string("ssh_connect: ") + ssh_get_error(m_libssh_session));
		return;
	}

	if(!m_ssh_settings.m_password.empty())
	{
		if(ssh_userauth_password(m_libssh_session, NULL, m_ssh_settings.m_password.c_str()) != SSH_AUTH_SUCCESS)
		{
			send_error(string("ssh_userauth_password: ") + ssh_get_error(m_libssh_session));
			return;
		}
	}
	else if(!m_ssh_settings.m_key.empty())
	{
		const char* passphrase = NULL;
		if(!m_ssh_settings.m_passphrase.empty())
		{
			passphrase = m_ssh_settings.m_passphrase.c_str();
		}

		if(ssh_pki_import_privkey_base64(m_ssh_settings.m_key.c_str(), passphrase, NULL, NULL, &m_libssh_key) != SSH_OK)
		{
			send_error("Error importing ssh key");
			return;
		}

		if(ssh_userauth_publickey(m_libssh_session, NULL, m_libssh_key) != SSH_AUTH_SUCCESS)
		{
			send_error(string("ssh_userauth_publickey: ") + ssh_get_error(m_libssh_session));
			return;
		}
	}
	else
	{
		send_error("No password or key specified");
		return;		
	}

	m_libssh_channel = ssh_channel_new(m_libssh_session);
	if(m_libssh_channel == NULL)
	{
		send_error("Error opening ssh channel");
		return;
	}

	if(ssh_channel_open_session(m_libssh_channel) != SSH_OK)
	{
		send_error(string("ssh_channel_open_session: ") + ssh_get_error(m_libssh_session));
		return;
	}

	if(ssh_channel_request_pty(m_libssh_channel) != SSH_OK)
	{
		send_error(string("ssh_channel_request_pty: ") + ssh_get_error(m_libssh_session));
		return;
	}

	if(ssh_channel_change_pty_size(m_libssh_channel, 80, 24) != SSH_OK)
	{
		send_error(string("ssh_channel_change_pty_size: ") + ssh_get_error(m_libssh_session));
		return;
	}

	if(ssh_channel_request_shell(m_libssh_channel) != SSH_OK)
	{
		send_error(string("ssh_channel_request_shell: ") + ssh_get_error(m_libssh_session));
		return;
	}

	m_last_activity_ns = dragent_configuration::get_current_time_ns();

	while(!dragent_configuration::m_terminate &&
		ssh_channel_is_open(m_libssh_channel) &&
		!ssh_channel_is_eof(m_libssh_channel))
	{
		if(dragent_configuration::get_current_time_ns() > 
			m_last_activity_ns + m_session_timeout_ns)
		{
			g_log->warning(m_name + ": SSH session timeout");
			break;
		}

		pending_message message;
		if(get_pending_messages(m_token, &message))
		{
			if(message.m_close)
			{
				g_log->information("Received SSH close message, aborting session");
				break;
			}

			if(!message.m_input.empty())
			{
				m_last_activity_ns = dragent_configuration::get_current_time_ns();
				write_to_channel(message.m_input);
			}
		}

		string output;
		read_from_channel(&output, false);
		read_from_channel(&output, true);

		if(output.size())
		{
			draiosproto::ssh_data response;
			prepare_response(&response);
			response.set_data(output);

			g_log->information(m_name + ": Sending partial output (" 
				+ NumberFormatter::format(output.size()) + ")");

			queue_response(response);
		}

		Thread::sleep(100);
		continue;
	}

	if(ssh_channel_is_open(m_libssh_channel))
	{
		ssh_channel_close(m_libssh_channel);
	}

	int exit_status = ssh_channel_get_exit_status(m_libssh_channel);

	g_log->information("SSH session terminated, exit code " + NumberFormatter::format(exit_status));

	draiosproto::ssh_data response;
	prepare_response(&response);
	response.set_exit_status(exit_status);

	g_log->information(m_name + ": Terminating");
}

void ssh_worker::send_error(const string& error)
{
	g_log->error(error);
	draiosproto::ssh_data response;
	prepare_response(&response);
	response.set_error(error);
	queue_response(response);
}

void ssh_worker::prepare_response(draiosproto::ssh_data* response)
{
	response->set_timestamp_ns(dragent_configuration::get_current_time_ns());
	response->set_customer_id(m_configuration->m_customer_id);
	response->set_machine_id(m_configuration->m_machine_id);
	response->set_token(m_token);
}

void ssh_worker::queue_response(const draiosproto::ssh_data& response)
{
	SharedPtr<protocol_queue_item> buffer = dragent_protocol::message_to_buffer(
		draiosproto::message_type::SSH_DATA, 
		response, 
		m_configuration->m_compression_enabled);

	if(buffer.isNull())
	{
		g_log->error("NULL converting message to buffer");
		return;
	}

	while(!m_queue->put(buffer))
	{
		g_log->error(m_name + ": Queue full, waiting");
		Thread::sleep(1000);

		if(dragent_configuration::m_terminate)
		{
			break;
		}
	}
}

void ssh_worker::read_from_channel(string* output, bool std_err)
{
	char buffer[8192];

	while(true)
	{
		int res = ssh_channel_read_nonblocking(m_libssh_channel, buffer, sizeof(buffer), std_err);
		if(res == SSH_ERROR)
		{
			ASSERT(false);
			break;
		}

		if(res == 0 || res == SSH_EOF)
		{
			break;
		}

		output->append(buffer, res);
	}
}

void ssh_worker::write_to_channel(const string& input)
{
	int n = 0;
	while(n != (int) input.size())
	{
		int res = ssh_channel_write(m_libssh_channel, input.data() + n, input.size() - n);
		if(res == SSH_ERROR)
		{
			ASSERT(false);
		}

		if(res == SSH_EOF)
		{
			break;
		}

		n += res;
	}
}

bool ssh_worker::get_pending_messages(const string& token, pending_message *message)
{
	Poco::Mutex::ScopedLock lock(m_pending_messages_lock);

	map<string, pending_message>::iterator it = m_pending_messages.find(token);
	if(it != m_pending_messages.end())
	{
		*message = it->second;
		it->second.m_input.clear();
		return true;
	}

	return false;
}

void ssh_worker::request_input(const string& token, const string& input)
{
	Poco::Mutex::ScopedLock lock(m_pending_messages_lock);

	g_log->information("Adding new input to session " + token);

	map<string, pending_message>::iterator it = m_pending_messages.find(token);
	if(it != m_pending_messages.end())
	{
		it->second.m_input.append(input);
	}
}

void ssh_worker::add_pending_messages(const string& token)
{
	Poco::Mutex::ScopedLock lock(m_pending_messages_lock);

	m_pending_messages.insert(pair<string, pending_message>(token, pending_message()));
}

void ssh_worker::delete_pending_messages(const string& token)
{
	Poco::Mutex::ScopedLock lock(m_pending_messages_lock);

	m_pending_messages.erase(token);
}

void ssh_worker::request_close(const string& token)
{
	Poco::Mutex::ScopedLock lock(m_pending_messages_lock);

	map<string, pending_message>::iterator it = m_pending_messages.find(token);
	if(it != m_pending_messages.end())
	{
		it->second.m_close = true;
	}
}
