#pragma once

#include <Poco/Message.h>
#include <Poco/RWLock.h>
#include <string>
#include <sstream>
#include <memory>
#include <list>
#include <map>
#include <atomic>
#include <ctime>
#include "common_logger.h"
#include "draios.pb.h"
#include "sinsp.h"
#include "sinsp_int.h"
#include "procfs_parser.h"
#include "type_config.h"

namespace draiosproto
{
	class statsd_info;
}

//
// Class for collecting agent and subprocesses internal metrics.
//
// This class is designed to function without locking in cases where access
// from multiple threads is needed (eg. log counters),
// on platforms where atomic integer operations are not lock-free, there
// may be locking, but this should be a rare occasion
//
// The trade-off associated with that is thet small counter errors may occur
// (eg. counter being off-by-one). Such inaccuracy is acceptable, eg. instead
// of 100 log entries, we may see 99
//
class internal_metrics : public common_logger::log_observer
{
public:
	typedef std::shared_ptr<internal_metrics> sptr_t;
	typedef std::map<std::string,uint64_t> subprocs_t;

	internal_metrics();

	// logger-related interface; thread-safe (only accesses atomic member variables)
	// !!! WARNING !!!: never log using global logger from this function,
	// logger will call it back and overflow the stack
	void notify(Poco::Message::Priority sev) override;

	uint64_t logs() const
	{
		return m_log.err + m_log.warn + m_log.info + m_log.debug;
	}

	// analyzer-related interface; not thread-safe, should only be accessed from single thread
	void set_process(int64_t val);
	void set_thread(int64_t val);
	void set_thread_drops(int64_t val);
	void set_container(int64_t val);
	void set_javaproc(int64_t val);
	void set_appcheck(int64_t val);
	void set_mesos_autodetect(bool flag);
	void set_mesos_detected(bool flag);
	void set_fp(int64_t val);
	void set_fl(int64_t val);
	void set_sr(int64_t val);
	void set_analyzer_cpu_percentage(int64_t val);

	// These are all from the inspector's get_capture_stats()
	void set_n_evts(int64_t val);
	void set_n_drops(int64_t val);
	void set_n_drops_buffer(int64_t val);
	void set_n_preemptions(int64_t val);
	void set_n_proc_lookups(int64_t val);
	void set_n_main_thread_lookups(int64_t val);

	void set_n_command_lines(int64_t val);
	void set_command_categories(std::map<draiosproto::command_category,uint64_t> &cats);

	void set_baseliner_enabled(bool val);

	int64_t get_process() const;
	int64_t get_thread() const;
	int64_t get_thread_drops() const;
	int64_t get_container() const;
	int64_t get_javaproc() const;
	int64_t get_appcheck() const;
	bool get_mesos_autodetect() const;
	bool get_mesos_detected() const;
	int64_t get_fp() const;
	int64_t get_fl() const;
	int64_t get_sr() const;
	int64_t get_analyzer_cpu_percentage() const;

	int64_t get_n_evts() const;
	int64_t get_n_drops() const;
	int64_t get_n_drops_buffer() const;
	int64_t get_n_preemptions() const;

	int64_t get_n_command_lines() const;
	std::map<draiosproto::command_category,uint64_t> &get_command_categories() const;

	// subprocesses-related interface
	void set_agent_cpu(int64_t val);
	void set_agent_memory(int64_t val);
	void set_java_cpu(int64_t val);
	void set_java_memory(int64_t val);
	void set_appcheck_cpu(int64_t val);
	void set_appcheck_memory(int64_t val);
	void set_mountedfs_reader_cpu(int64_t val);
	void set_mountedfs_reader_memory(int64_t val);
	void set_statsite_forwarder_cpu(int64_t val);
	void set_statsite_forwarder_memory(int64_t val);
	void set_cointerface_cpu(int64_t val);
	void set_cointerface_memory(int64_t val);
	void set_secure_audit_n_sent_protobufs(int64_t val);
	void set_secure_audit_fl_ms(int64_t val);
	void set_secure_audit_emit_ms(int64_t val);
	void set_secure_audit_executed_commands_count(int64_t val);
	void set_secure_audit_connections_count(int64_t val);
	void set_secure_audit_k8s_count(int64_t val);
	void set_secure_audit_executed_commands_dropped_count(int64_t val);
	void set_secure_audit_connections_dropped_count(int64_t val);
	void set_secure_audit_k8s_dropped_count(int64_t val);
	void set_secure_audit_connections_not_interactive_dropped(int64_t val);
	void set_secure_audit_k8s_enrich_errors(int64_t val);

	// Set the process ids associated with the various
	// subprocesses
	void set_subprocesses(subprocs_t &subprocesses);

	// Update internal metrics for the set of subprocesses
	void update_subprocess_metrics(sinsp_procfs_parser *procfs_parser);

	int64_t get_agent_cpu() const;
	int64_t get_agent_memory() const;
	int64_t get_java_cpu() const;
	int64_t get_java_memory() const;
	int64_t get_appcheck_cpu() const;
	int64_t get_appcheck_memory() const;
	int64_t get_mountedfs_reader_cpu() const;
	int64_t get_mountedfs_reader_memory() const;
	int64_t get_statsite_forwarder_cpu() const;
	int64_t get_statsite_forwarder_memory() const;
	int64_t get_cointerface_cpu() const;
	int64_t get_cointerface_memory() const;
	int64_t get_secure_audit_n_sent_protobufs() const;
	int64_t get_secure_audit_fl_ms() const;
	int64_t get_secure_audit_emit_ms() const;
	int64_t get_secure_audit_executed_commands_count() const;
	int64_t get_secure_audit_connections_count() const;
	int64_t get_secure_audit_k8s_count() const;
	int64_t get_secure_audit_executed_commands_dropped_count() const;
	int64_t get_secure_audit_connections_dropped_count() const;
	int64_t get_secure_audit_k8s_dropped_count() const;
	int64_t get_secure_audit_connections_not_interactive_dropped() const;
	int64_t get_secure_audit_k8s_enrich_errors() const;

	// For other metrics sources e.g. security manager event
	// counts, you can provide objects derived from this type and
	// maintain the counts in that object.
	class ext_source
	{
	public:
		virtual void send_all(draiosproto::statsd_info *statsd_info) = 0;
		virtual void send_some(draiosproto::statsd_info *statsd_info) = 0;
	};

	void add_ext_source(ext_source *src);

	/**
	 * emit the status of the internal metrics to the provided protobuf
	 *
	 * exactly which stats are emitted depends on configuration
	 *
	 * @param[in|out] the protobuf to stuff the metrics in
	 * @param[in] capture_stats current capture stats
	 * @param[in] flush_share the proportion of time which the analyzer spent in flush
	 * @param[in] sampling_ratio the sampling ratio of the last sample
	 * @param[in] flush_duration_ns the time taken during the previous flush
	 */
	void emit(draiosproto::statsd_info* statsd_info,
		  const scap_stats& capture_stats,
		  double flush_share,
		  uint32_t sampling_ratio,
		  uint64_t flush_duration_ns,
		  uint64_t n_proc_lookups,
		  uint64_t n_main_thread_lookups,
		  uint64_t analyzer_cpu_percentage);

private: // helper methods used during emit
	// adds statsd-emulated metrics directly to protobuf
	// returns false if statsd_info is null
	bool send_all(draiosproto::statsd_info* statsd_info);

	// Add a limited set of metrics to the provided protobuf.
	bool send_some(draiosproto::statsd_info* statsd_info);

public:
	template<typename T>
	static draiosproto::statsd_metric* write_metric(draiosproto::statsd_info* statsd_info,
							const std::string& name,
							const std::map<std::string,std::string> &tags,
							draiosproto::statsd_metric_type type, const T& val)
	{
		// don't clog protobuf with values that have not been set yet
		if(-1 != (int) val)
		{
			draiosproto::statsd_metric* proto = statsd_info->add_statsd_metrics();
			proto->set_name(name);
			for(auto &pair : tags)
			{
				draiosproto::statsd_tag* tag = proto->add_tags();
				tag->set_key(pair.first);
				tag->set_value(pair.second);
			}

			proto->set_type(type);
			proto->set_value(val);
			if(g_logger.get_severity() >= sinsp_logger::SEV_TRACE)
			{
				g_logger.log("Internal metric [" + name + "], value=" + std::to_string(val), sinsp_logger::SEV_TRACE);
			}
			return proto;
		}
		if(g_logger.get_severity() >= sinsp_logger::SEV_TRACE)
		{
			g_logger.log("Internal metric [" + name + "] not set, value=" + std::to_string(val), sinsp_logger::SEV_TRACE);
		}
		return nullptr;
	}

	template<typename T>
	static draiosproto::statsd_metric* write_metric(draiosproto::statsd_info* statsd_info,
							const std::string& name,
							draiosproto::statsd_metric_type type, const T& val)
	{
		std::map<std::string,std::string> tags;

		return write_metric(statsd_info, name, tags, type, val);
	}

	// resets all accumulated values
	// true gauges are left intact
	void reset();

private:

	struct log
	{
		log(): err(0), warn(0), info(0), debug(0)
		{}
		// must be atomic because accessed from multiple threads
		std::atomic<int64_t> err;
		std::atomic<int64_t> warn;
		std::atomic<int64_t> info;
		std::atomic<int64_t> debug;
	};

	struct analyzer
	{
		analyzer(): agent_cpu(-1)
			,agent_memory(-1)
			,java_cpu(-1)
			,java_memory(-1)
			,appcheck_cpu(-1)
			,appcheck_memory(-1)
			,mountedfs_reader_cpu(-1)
			,mountedfs_reader_memory(-1)
			,statsite_forwarder_cpu(-1)
			,statsite_forwarder_memory(-1)
			,cointerface_cpu(-1)
			,cointerface_memory(-1)
		{}
		// all accessed from single thread, so no need for atomic;
		// if there ever a need for multi-thread access arises,
		// integers will have to be turned into atomics and any
		// arrays or STL containers will have to be sent to statsd
		// (instead of aggregated internally)
		int64_t process_cnt = -1;
		int64_t thread_cnt = -1;
		int64_t thread_drop_cnt = -1;
		int64_t container_cnt = -1;
		int64_t javaproc_cnt = -1;
		int64_t appcheck_cnt = -1;
		bool mesos_autodetect = false;
		bool mesos_detected = false;
		int64_t fp = -1;
		int64_t fl = -1;
		int64_t sr = -1;
		double analyzer_cpu_percentage = -1;

		int64_t n_evts = -1;
		int64_t n_drops = -1;
		int64_t n_drops_buffer = -1;
		int64_t n_preemptions = -1;
		int64_t n_proc_lookups = -1;
		int64_t n_main_thread_lookups = -1;

		int64_t n_command_lines = -1;
		std::map<draiosproto::command_category,uint64_t> m_command_categories;

		bool baseliner_enabled = false;

		int64_t secure_audit_n_sent_protobufs = -1;
		int64_t secure_audit_fl_ms = -1;
		int64_t secure_audit_emit_ms = -1;

		int64_t secure_audit_executed_commands_count = -1;
		int64_t secure_audit_connections_count = -1;
		int64_t secure_audit_k8s_count = -1;
		int64_t secure_audit_executed_commands_dropped_count = -1;
		int64_t secure_audit_connections_dropped_count = -1;
		int64_t secure_audit_k8s_dropped_count = -1;
		int64_t secure_audit_connections_not_interactive_dropped = -1;
		int64_t secure_audit_k8s_enrich_errors = -1;

		int64_t agent_cpu;
		int64_t agent_memory;
		int64_t java_cpu;
		int64_t java_memory;
		int64_t appcheck_cpu;
		int64_t appcheck_memory;
		int64_t mountedfs_reader_cpu;
		int64_t mountedfs_reader_memory;
		int64_t statsite_forwarder_cpu;
		int64_t statsite_forwarder_memory;
		int64_t cointerface_cpu;
		int64_t cointerface_memory;

		// Can be set/read from multiple threads, so protected by a read/write lock.
		subprocs_t subprocs;
		Poco::RWLock subprocs_lock;

		std::unordered_map<pid_t, uint64_t> subprocs_old_jiffies;
	};

	void send_command_categories(draiosproto::statsd_info* statsd_info);
	void send_secure_audit_metrics(draiosproto::statsd_info* statsd_info);

	analyzer m_analyzer;
	std::list<ext_source *> m_ext_sources;
	log m_log;

public: // configs
	static type_config<bool> c_extra_internal_metrics;

	friend class test_helper;
};

inline void internal_metrics::set_process(int64_t val)
{
	m_analyzer.process_cnt = val;
}

inline void internal_metrics::set_thread(int64_t val)
{
	m_analyzer.thread_cnt = val;
}

inline void internal_metrics::set_thread_drops(int64_t val)
{
	m_analyzer.thread_drop_cnt = val;
}

inline void internal_metrics::set_container(int64_t val)
{
	m_analyzer.container_cnt = val;
}

inline void internal_metrics::set_javaproc(int64_t val)
{
	m_analyzer.javaproc_cnt = val;
}

inline void internal_metrics::set_appcheck(int64_t val)
{
	m_analyzer.appcheck_cnt = val;
}

inline void internal_metrics::set_mesos_autodetect(bool flag)
{
	m_analyzer.mesos_autodetect = flag;
}

inline void internal_metrics::set_mesos_detected(bool flag)
{
	m_analyzer.mesos_detected = flag;
}

inline void internal_metrics::set_agent_cpu(int64_t val)
{
	m_analyzer.agent_cpu = val;
}

inline void internal_metrics::set_agent_memory(int64_t val)
{
	m_analyzer.agent_memory = val;
}

inline void internal_metrics::set_java_cpu(int64_t val)
{
	m_analyzer.java_cpu = val;
}

inline void internal_metrics::set_java_memory(int64_t val)
{
	m_analyzer.java_memory = val;
}

inline void internal_metrics::set_appcheck_cpu(int64_t val)
{
	m_analyzer.appcheck_cpu = val;
}

inline void internal_metrics::set_appcheck_memory(int64_t val)
{
	m_analyzer.appcheck_memory = val;
}

inline void internal_metrics::set_mountedfs_reader_cpu(int64_t val)
{
	m_analyzer.mountedfs_reader_cpu = val;
}

inline void internal_metrics::set_mountedfs_reader_memory(int64_t val)
{
	m_analyzer.mountedfs_reader_memory = val;
}

inline void internal_metrics::set_statsite_forwarder_cpu(int64_t val)
{
	m_analyzer.statsite_forwarder_cpu = val;
}

inline void internal_metrics::set_statsite_forwarder_memory(int64_t val)
{
	m_analyzer.statsite_forwarder_memory = val;
}

inline void internal_metrics::set_cointerface_cpu(int64_t val)
{
	m_analyzer.cointerface_cpu = val;
}

inline void internal_metrics::set_cointerface_memory(int64_t val)
{
	m_analyzer.cointerface_memory = val;
}

inline void internal_metrics::set_fp(int64_t val)
{
	m_analyzer.fp = val;
}

inline void internal_metrics::set_fl(int64_t val)
{
	m_analyzer.fl = val;
}

inline void internal_metrics::set_sr(int64_t val)
{
	m_analyzer.sr = val;
}

inline void internal_metrics::set_analyzer_cpu_percentage(int64_t val)
{
	m_analyzer.analyzer_cpu_percentage = val;
}

inline void internal_metrics::set_n_evts(int64_t val)
{
	m_analyzer.n_evts = val;
}

inline void internal_metrics::set_n_drops(int64_t val)
{
	m_analyzer.n_drops = val;
}

inline void internal_metrics::set_n_drops_buffer(int64_t val)
{
	m_analyzer.n_drops_buffer = val;
}

inline void internal_metrics::set_n_preemptions(int64_t val)
{
	m_analyzer.n_preemptions = val;
}

inline void internal_metrics::set_n_proc_lookups(int64_t val)
{
	m_analyzer.n_proc_lookups = val;
}

inline void internal_metrics::set_n_main_thread_lookups(int64_t val)
{
	m_analyzer.n_main_thread_lookups = val;
}

inline void internal_metrics::set_n_command_lines(int64_t val)
{
	m_analyzer.n_command_lines = val;
}

inline void internal_metrics::set_command_categories(std::map<draiosproto::command_category,uint64_t> &cats)
{
	m_analyzer.m_command_categories = cats;
}

inline void internal_metrics::set_baseliner_enabled(bool val)
{
	m_analyzer.baseliner_enabled = val;
}

inline int64_t internal_metrics::get_process() const
{
	return m_analyzer.process_cnt;
}

inline int64_t internal_metrics::get_thread() const
{
	return m_analyzer.thread_cnt;
}

inline int64_t internal_metrics::get_thread_drops() const
{
	return m_analyzer.thread_drop_cnt;
}

inline int64_t internal_metrics::get_container() const
{
	return m_analyzer.container_cnt;
}

inline int64_t internal_metrics::get_javaproc() const
{
	return m_analyzer.javaproc_cnt;
}

inline int64_t internal_metrics::get_appcheck() const
{
	return m_analyzer.appcheck_cnt;
}

inline bool internal_metrics::get_mesos_autodetect() const
{
	return m_analyzer.mesos_autodetect;
}

inline bool internal_metrics::get_mesos_detected() const
{
	return m_analyzer.mesos_detected;
}

inline int64_t internal_metrics::get_fp() const
{
	return m_analyzer.fp;
}

inline int64_t internal_metrics::get_fl() const
{
	return m_analyzer.fl;
}

inline int64_t internal_metrics::get_sr() const
{
	return m_analyzer.sr;
}

inline int64_t internal_metrics::get_analyzer_cpu_percentage() const
{
	return m_analyzer.analyzer_cpu_percentage;
}

inline int64_t internal_metrics::get_n_evts() const
{
	return m_analyzer.n_evts;
}

inline int64_t internal_metrics::get_n_drops() const
{
	return m_analyzer.n_drops;
}

inline int64_t internal_metrics::get_n_drops_buffer() const
{
	return m_analyzer.n_drops_buffer;
}

inline int64_t internal_metrics::get_n_preemptions() const
{
	return m_analyzer.n_preemptions;
}

inline int64_t internal_metrics::get_n_command_lines() const
{
	return m_analyzer.n_command_lines;
}

inline int64_t internal_metrics::get_agent_cpu() const
{
	return m_analyzer.agent_cpu;
}

inline int64_t internal_metrics::get_agent_memory() const
{
	return m_analyzer.agent_memory;
}

inline int64_t internal_metrics::get_java_cpu() const
{
	return m_analyzer.java_cpu;
}

inline int64_t internal_metrics::get_java_memory() const
{
	return m_analyzer.java_memory;
}

inline int64_t internal_metrics::get_appcheck_cpu() const
{
	return m_analyzer.appcheck_cpu;
}

inline int64_t internal_metrics::get_appcheck_memory() const
{
	return m_analyzer.appcheck_memory;
}

inline int64_t internal_metrics::get_mountedfs_reader_cpu() const
{
	return m_analyzer.mountedfs_reader_cpu;
}

inline int64_t internal_metrics::get_mountedfs_reader_memory() const
{
	return m_analyzer.mountedfs_reader_memory;
}

inline int64_t internal_metrics::get_statsite_forwarder_cpu() const
{
	return m_analyzer.statsite_forwarder_cpu;
}

inline int64_t internal_metrics::get_statsite_forwarder_memory() const
{
	return m_analyzer.statsite_forwarder_memory;
}

inline int64_t internal_metrics::get_cointerface_cpu() const
{
	return m_analyzer.cointerface_cpu;
}

inline int64_t internal_metrics::get_cointerface_memory() const
{
	return m_analyzer.cointerface_memory;
}

inline int64_t internal_metrics::get_secure_audit_n_sent_protobufs() const
{
	return m_analyzer.secure_audit_n_sent_protobufs;
}

inline void internal_metrics::set_secure_audit_n_sent_protobufs(int64_t val)
{
	m_analyzer.secure_audit_n_sent_protobufs = val;
}

inline int64_t internal_metrics::get_secure_audit_fl_ms() const
{
	return m_analyzer.secure_audit_fl_ms;
}

inline void internal_metrics::set_secure_audit_fl_ms(int64_t val)
{
	m_analyzer.secure_audit_fl_ms = val;
}

inline int64_t internal_metrics::get_secure_audit_emit_ms() const
{
	return m_analyzer.secure_audit_emit_ms;
}

inline void internal_metrics::set_secure_audit_emit_ms(int64_t val)
{
	m_analyzer.secure_audit_emit_ms = val;
}

inline void internal_metrics::set_secure_audit_executed_commands_count(int64_t val)
{
	m_analyzer.secure_audit_executed_commands_count = val;
}

inline void internal_metrics::set_secure_audit_connections_count(int64_t val)
{
	m_analyzer.secure_audit_connections_count = val;
}

inline void internal_metrics::set_secure_audit_k8s_count(int64_t val)
{
	m_analyzer.secure_audit_k8s_count = val;
}

inline void internal_metrics::set_secure_audit_executed_commands_dropped_count(int64_t val)
{
	m_analyzer.secure_audit_executed_commands_dropped_count = val;
}

inline void internal_metrics::set_secure_audit_connections_dropped_count(int64_t val)
{
	m_analyzer.secure_audit_connections_dropped_count = val;
}

inline void internal_metrics::set_secure_audit_k8s_dropped_count(int64_t val)
{
	m_analyzer.secure_audit_k8s_dropped_count = val;
}

inline void internal_metrics::set_secure_audit_connections_not_interactive_dropped(int64_t val)
{
	m_analyzer.secure_audit_connections_not_interactive_dropped = val;
}

inline void internal_metrics::set_secure_audit_k8s_enrich_errors(int64_t val)
{
	m_analyzer.secure_audit_k8s_enrich_errors = val;
}

inline int64_t internal_metrics::get_secure_audit_executed_commands_count() const
{
	return m_analyzer.secure_audit_executed_commands_count;
}

inline int64_t internal_metrics::get_secure_audit_connections_count() const
{
	return m_analyzer.secure_audit_connections_count;
}

inline int64_t internal_metrics::get_secure_audit_k8s_count() const
{
	return m_analyzer.secure_audit_k8s_count;
}

inline int64_t internal_metrics::get_secure_audit_executed_commands_dropped_count() const
{
	return m_analyzer.secure_audit_executed_commands_dropped_count;
}

inline int64_t internal_metrics::get_secure_audit_connections_dropped_count() const
{
	return m_analyzer.secure_audit_connections_dropped_count;
}

inline int64_t internal_metrics::get_secure_audit_k8s_dropped_count() const
{
	return m_analyzer.secure_audit_k8s_dropped_count;
}

inline int64_t internal_metrics::get_secure_audit_connections_not_interactive_dropped() const
{
	return m_analyzer.secure_audit_connections_not_interactive_dropped;
}

inline int64_t internal_metrics::get_secure_audit_k8s_enrich_errors() const
{
	return m_analyzer.secure_audit_k8s_enrich_errors;
}
