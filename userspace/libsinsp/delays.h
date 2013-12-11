#pragma once

class sinsp_program_percpu_delays
{
public:
	void clear()
	{
		m_last_inbound_transaction_union.clear();
		m_last_outbound_transaction_union.clear();
	}

	vector<sinsp_trlist_entry> m_last_inbound_transaction_union;
	vector<sinsp_trlist_entry> m_last_outbound_transaction_union;
	uint64_t m_total_merged_inbound_delay;
	uint64_t m_total_merged_outbound_delay;
};

class sinsp_program_delays
{
public:
	vector<sinsp_program_percpu_delays> m_last_prog_delays;
	float m_local_remote_ratio;
	uint64_t m_transaction_processing_delay_ns;
};

//
// The main analyzer class
//
class sinsp_delays
{
public:
	sinsp_delays(sinsp_analyzer* analyzer, uint32_t ncpus);
	uint64_t compute_thread_transaction_delay(sinsp_transaction_counters* trcounters);
	void compute_host_transaction_delay(sinsp_transaction_counters* counters);
	static void merge_intervals(vector<sinsp_trlist_entry>* intervals, OUT vector<sinsp_trlist_entry>* s, OUT uint64_t* tot_time, int64_t progid);

	sinsp_program_delays* compute_program_delays(sinsp_threadinfo* program_info);

private:
	void compute_program_cpu_delays(sinsp_threadinfo* program_info, int32_t cpuid);

	sinsp* m_inspector;
	sinsp_analyzer* m_analyzer;
	int32_t m_num_cpus;
	sinsp_program_delays m_last_prog_delays;
};
