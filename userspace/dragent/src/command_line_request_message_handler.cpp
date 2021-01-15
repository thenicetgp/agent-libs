#include "command_line_request_message_handler.h"
#include "common_logger.h"
#include "protocol.h"
#include <string>

COMMON_LOGGER();

draiosproto::command_line_content_type to_proto(command_line_manager::content_type type)
{
	switch (type) 
	{
	case command_line_manager::content_type::TEXT:
		return draiosproto::command_line_content_type::CLI_HTML_TEXT;
	case command_line_manager::content_type::JSON:
		return draiosproto::command_line_content_type::CLI_JSON;
	case command_line_manager::content_type::YAML:
		return draiosproto::command_line_content_type::CLI_YAML;
	case command_line_manager::content_type::ERROR:
		return draiosproto::command_line_content_type::CLI_ERROR;
	}

	return draiosproto::command_line_content_type::CLI_ERROR;
}

namespace dragent
{

command_line_request_message_handler::command_line_request_message_handler(
	const std::shared_ptr<async_command_handler> &handler,
	message_transmitter& transmitter,
	const dragent_configuration& configuration) :
		m_handler(handler),
		m_transmitter(transmitter),
		m_configuration(configuration)
{}

bool command_line_request_message_handler::handle_message(const draiosproto::message_type type,
                                                          const uint8_t* const buffer,
                                                          const size_t buffer_size)
{
	if (draiosproto::message_type::COMMAND_LINE_REQUEST != type) 
	{
		LOG_ERROR("Unexpected message type: %d", type);
		return false;
	}

	draiosproto::command_line_request request;
	dragent_protocol::buffer_to_protobuf(buffer, buffer_size, &request);

	if (!m_handler) 
	{
		LOG_INFO("Command line is disabled.");
		command_line_manager::response resp;
		resp.first = command_line_manager::content_type::ERROR;
		resp.second = "Agent CLI is disabled in agent configuration.";
		send_response(request.key(), resp);
		return true;
	}

	if(!request.has_permissions())
	{
		LOG_INFO("Permissions not set.");
		command_line_manager::response resp;
		resp.first = command_line_manager::content_type::ERROR;
		resp.second = "Permissions must be set.";
		send_response(request.key(), resp);
		return true;
	}

	auto cb = std::bind(&command_line_request_message_handler::send_response,
			    this,
			    request.key(),
			    std::placeholders::_1);
	auto permissions = to_permissions(request.permissions());
	m_handler->async_handle_command(permissions, request.command(), cb);

	return true;
}

command_line_permissions command_line_request_message_handler::to_permissions(const draiosproto::command_line_permissions& msg)
{
	command_line_permissions permissions;
	if (msg.has_agent_status() && msg.agent_status()) 
	{
		permissions.push_back(CLI_AGENT_STATUS);
	}
	if (msg.has_agent_internal_diagnostics() && msg.agent_internal_diagnostics()) 
	{
		permissions.push_back(CLI_AGENT_INTERNAL_DIAGNOSTICS);
	}
	if (msg.has_network_calls_to_remote_pods() && msg.network_calls_to_remote_pods()) 
	{
		permissions.push_back(CLI_NETWORK_CALLS_TO_REMOTE_PODS);
	}
	if (msg.has_view_configuration() && msg.view_configuration()) 
	{
		permissions.push_back(CLI_VIEW_CONFIGURATION);
	}
	if (msg.has_view_sensitive_configuration() && msg.view_sensitive_configuration()) 
	{
		permissions.push_back(CLI_VIEW_SENSITIVE_CONFIGURATION);
	}

	return permissions;
}

void command_line_request_message_handler::send_response(
    const std::string& key,
    const command_line_manager::response& cmd_response)
{
	draiosproto::command_line_response response;
	response.set_key(key);
	response.set_content_type(to_proto(cmd_response.first));
	response.set_response(cmd_response.second);
	response.set_machine_id(m_configuration.machine_id());
	response.set_customer_id(m_configuration.m_customer_id);

	m_transmitter.transmit(draiosproto::message_type::COMMAND_LINE_RESPONSE,
	                       response);
}

} // namespace dragent
