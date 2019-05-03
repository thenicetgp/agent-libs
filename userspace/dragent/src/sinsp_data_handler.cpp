#include <memory>

#include "sinsp_data_handler.h"
#include "configuration.h"
#include "utils.h"
#include "logger.h"

#include "draios.pb.h"
#include "tap.pb.h"

DRAGENT_LOGGER();

sinsp_data_handler::sinsp_data_handler(const dragent_configuration* configuration,
				       protocol_queue* queue) :
	m_configuration(configuration),
	m_queue(queue),
	m_last_loop_ns(0)
{
}

sinsp_data_handler::~sinsp_data_handler()
{
}

void sinsp_data_handler::sinsp_analyzer_data_ready(uint64_t ts_ns,
						   uint64_t nevts,
						   uint64_t num_drop_events,
						   draiosproto::metrics* metrics,
						   uint32_t sampling_ratio,
						   double analyzer_cpu_pct,
						   double flush_cpu_pct,
						   uint64_t analyzer_flush_duration_ns,
						   uint64_t num_suppressed_threads)
{
	m_last_loop_ns = sinsp_utils::get_current_time_ns();

	if(m_configuration->m_print_protobuf)
	{
		LOG_INFO(metrics->DebugString());
	}

	std::shared_ptr<protocol_queue_item> buffer = dragent_protocol::message_to_buffer(
		ts_ns,
		draiosproto::message_type::METRICS,
		*metrics,
		m_configuration->m_compression_enabled);

	if(!buffer)
	{
		LOG_ERROR("NULL converting message to buffer");
		return;
	}

	// The following message was provided to Goldman Sachs (Oct 2018). Do not change.
	LOG_INFO("ts=" + NumberFormatter::format(ts_ns / 1000000000)
		+ ", len=" + NumberFormatter::format(buffer->buffer.size())
		+ ", ne=" + NumberFormatter::format(nevts)
                + ", de=" + NumberFormatter::format(num_drop_events)
 		+ ", c=" + NumberFormatter::format(analyzer_cpu_pct, 2)
		+ ", fp=" + NumberFormatter::format(flush_cpu_pct, 2)
 		+ ", sr=" + NumberFormatter::format(sampling_ratio)
 		+ ", st=" + NumberFormatter::format(num_suppressed_threads)
 		+ ", fl=" + NumberFormatter::format(analyzer_flush_duration_ns / 1000000));

	if(!m_queue->put(buffer, protocol_queue::BQ_PRIORITY_MEDIUM))
	{
		LOG_INFO("Queue full, discarding sample");
	}
}

void sinsp_data_handler::security_mgr_policy_events_ready(uint64_t ts_ns, draiosproto::policy_events *events)
{
	if(m_configuration->m_print_protobuf)
	{
		LOG_INFO(string("Security Events:") + events->DebugString());
	}

	std::shared_ptr<protocol_queue_item> buffer = dragent_protocol::message_to_buffer(
		ts_ns,
		draiosproto::message_type::POLICY_EVENTS,
		*events,
		m_configuration->m_compression_enabled);

	if(!buffer)
	{
		LOG_ERROR("NULL converting message to buffer");
		return;
	}

	LOG_INFO("sec_evts len=" + NumberFormatter::format(buffer->buffer.size())
			   + ", ne=" + NumberFormatter::format(events->events_size()));

	if(!m_queue->put(buffer, protocol_queue::BQ_PRIORITY_MEDIUM))
	{
		LOG_INFO("Queue full, discarding sample");
	}
}

void sinsp_data_handler::security_mgr_throttled_events_ready(uint64_t ts_ns,
							     draiosproto::throttled_policy_events *tevents,
							     uint32_t total_throttled_count)
{
	if(m_configuration->m_print_protobuf)
	{
		LOG_INFO(string("Throttled Security Events:") + tevents->DebugString());
	}

	std::shared_ptr<protocol_queue_item> buffer = dragent_protocol::message_to_buffer(
		ts_ns,
		draiosproto::message_type::THROTTLED_POLICY_EVENTS,
		*tevents,
		m_configuration->m_compression_enabled);

	if(!buffer)
	{
		LOG_ERROR("NULL converting message to buffer");
		return;
	}

	LOG_INFO("sec_evts len=" + NumberFormatter::format(buffer->buffer.size())
			   + ", nte=" + NumberFormatter::format(tevents->events_size())
			   + ", tcount=" + NumberFormatter::format(total_throttled_count));

	if(!m_queue->put(buffer, protocol_queue::BQ_PRIORITY_LOW))
	{
		LOG_INFO("Queue full, discarding sample");
	}
}

void sinsp_data_handler::security_mgr_comp_results_ready(uint64_t ts_ns, const draiosproto::comp_results *results)
{
	if(m_configuration->m_print_protobuf)
	{
		LOG_INFO(string("Compliance Results:") + results->DebugString());
	}

	std::shared_ptr<protocol_queue_item> buffer = dragent_protocol::message_to_buffer(
		ts_ns,
		draiosproto::message_type::COMP_RESULTS,
		*results,
		m_configuration->m_compression_enabled);

	if(!buffer)
	{
		LOG_ERROR("NULL converting message to buffer");
		return;
	}

	LOG_INFO("sec_comp_results len=" + NumberFormatter::format(buffer->buffer.size())
			   + ", ne=" + NumberFormatter::format(results->results_size()));

	if(!m_queue->put(buffer, protocol_queue::BQ_PRIORITY_LOW))
	{
		LOG_INFO("Queue full, discarding sample");
	}
}

void sinsp_data_handler::audit_tap_data_ready(uint64_t ts_ns, const tap::AuditLog *audit_log)
{
	if(m_configuration->m_print_protobuf || m_configuration->m_audit_tap_debug_only)
	{
		LOG_INFO(string("Audit tap data:") + audit_log->DebugString());
	}

	std::shared_ptr<protocol_queue_item> buffer = dragent_protocol::message_to_buffer(
		ts_ns,
		draiosproto::message_type::AUDIT_TAP,
		*audit_log,
		true /* compression always enabled */);

	if(!buffer)
	{
		LOG_ERROR("NULL converting audit_tap message to buffer");
		return;
	}

	LOG_INFO("audit_tap len=" + NumberFormatter::format(buffer->buffer.size())
			   + ", np=" + NumberFormatter::format(audit_log->newprocessevents().size())
		           + ", pe=" + NumberFormatter::format(audit_log->processexitevents().size())
		           + ", c=" + NumberFormatter::format(audit_log->connectionevents().size())
			   + ", e=" + NumberFormatter::format(audit_log->environmentvariables().size())
			   );

	if(m_configuration->m_audit_tap_debug_only)
	{
		return;
	}

	if(!m_queue->put(buffer, protocol_queue::BQ_PRIORITY_MEDIUM))
	{
		LOG_INFO("Queue full, discarding sample");
	}
}