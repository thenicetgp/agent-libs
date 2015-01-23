#ifdef HAS_ANALYZER

#pragma once

#include "transactinfo.h"
#include "protostate.h"
#include "delays.h"

///////////////////////////////////////////////////////////////////////////////
// Information that is included only in processes that are main threads
///////////////////////////////////////////////////////////////////////////////
class sinsp_procinfo
{
public:
	void clear();
	uint64_t get_tot_cputime();

	// True if this process is outside the top list that goes in the sample.
	bool m_exclude_from_sample;
	// Aggreaged metrics for the process.
	// This field is allocated only for process main threads.
	sinsp_counters m_proc_metrics; 
	// Aggreaged transaction metrics for the process.
	// This field is allocated only for process main threads.
	sinsp_transaction_counters m_proc_transaction_metrics;
	// The ratio between the number of connections waiting to be served and 
	// the total connection queue length for this process.
	uint32_t m_connection_queue_usage_pct;
	// The ratio between open FDs and maximum available FDs fir this thread
	uint32_t m_fd_usage_pct;
	// the process capcity score calculated with our secret sauce algorithms
	float m_capacity_score;
	// the process capacity stolen by CPU steal time, calculated with our secret sauce algorithms
	float m_stolen_capacity_score;
	// the process CPU load
	double m_cpuload;
	// total virtual memory
	uint32_t m_vmsize_kb;
	// resident non-swapped memory
	uint32_t m_vmrss_kb;
	// swapped memory
	uint32_t m_vmswap_kb;
	// number of major page faults since start
	uint64_t m_pfmajor;
	// number of minor page faults since start
	uint64_t m_pfminor;
	// list of processes that are part of this program
#ifdef ANALYZER_EMITS_PROGRAMS
	set<int64_t> m_program_pids;
#endif
	// Number of child threads or processes that served transactions
	uint64_t m_n_transaction_threads;
	// The metrics for transaction coming from the external world
	sinsp_transaction_counters m_external_transaction_metrics; 
	// State for delay computation
	sinsp_delays_info m_transaction_delays;
	// Time spent by this process on each of the CPUs
	vector<uint64_t> m_cpu_time_ns;
	// Syscall error table
	sinsp_error_counters m_syscall_errors;
	// Completed transactions lists
	vector<vector<sinsp_trlist_entry>> m_server_transactions_per_cpu;
	vector<vector<sinsp_trlist_entry>> m_client_transactions_per_cpu;
	// The protocol state
	sinsp_protostate m_protostate;
	// Number of FDs
	uint32_t m_fd_count;
};

class thread_analyzer_dyn_state
{
public:
	// Time spent by this process on each of the CPUs
	vector<uint64_t> m_cpu_time_ns;
	// Syscall error table
	sinsp_error_counters m_syscall_errors;
	// Completed transactions lists
	vector<vector<sinsp_trlist_entry>> m_server_transactions_per_cpu;
	vector<vector<sinsp_trlist_entry>> m_client_transactions_per_cpu;
	// The protocol state
	sinsp_protostate m_protostate;
};

///////////////////////////////////////////////////////////////////////////////
// Thread-related analyzer state
///////////////////////////////////////////////////////////////////////////////
class thread_analyzer_info
{
public:
	//
	// thread flags
	//
	enum flags
	{
	    AF_NONE = 0,
	    AF_INVALID = (1 << 0),
	    AF_PARTIAL_METRIC = (1 << 1), // Used by the event analyzer to flag that part of the last event has already been measured because the sampling time elapsed
	    AF_IS_LOCAL_IPV4_SERVER = (1 << 2), // set if this thread serves IPv4 transactions coming from the same machine.
	    AF_IS_REMOTE_IPV4_SERVER = (1 << 3), // set if this thread serves IPv4 transactions coming from another machine.
	    AF_IS_UNIX_SERVER = (1 << 4), // set if this thread serves unix transactions.
	    AF_IS_LOCAL_IPV4_CLIENT = (1 << 5), // set if this thread creates IPv4 transactions toward localhost.
	    AF_IS_REMOTE_IPV4_CLIENT = (1 << 6), // set if this thread creates IPv4 transactions toward another host.
	    AF_IS_UNIX_CLIENT = (1 << 7), // set if this thread creates unix transactions.
	    AF_IS_MAIN_PROGRAM_THREAD = (1 << 8), // set for main program threads.
	};

	void init(sinsp *inspector, sinsp_threadinfo* tinfo);
	void destroy();
	const sinsp_counters* get_metrics();
	void allocate_procinfo_if_not_present();
	void propagate_flag(flags flags, thread_analyzer_info* other);
	void propagate_flag_bidirectional(flags flag, thread_analyzer_info* other);
	void add_all_metrics(thread_analyzer_info* other);
	void clear_all_metrics();
	void clear_role_flags();
	void flush_inactive_transactions(uint64_t sample_end_time, uint64_t timeout_ns, bool is_subsampling);
	void add_completed_server_transaction(sinsp_partial_transaction* tr, bool isexternal);
	void add_completed_client_transaction(sinsp_partial_transaction* tr, bool isexternal);
	inline bool is_main_program_thread()
	{
		return (m_th_analysis_flags & AF_IS_MAIN_PROGRAM_THREAD) != 0;
	}

	inline void set_main_program_thread(bool is_main_program_thread)
	{
		if(is_main_program_thread)
		{
			m_th_analysis_flags |= AF_IS_MAIN_PROGRAM_THREAD;
		}
		else
		{
			m_th_analysis_flags &= ~AF_IS_MAIN_PROGRAM_THREAD;
		}
	}

	// Global state
	sinsp *m_inspector;
	sinsp_analyzer* m_analyzer;
	sinsp_threadinfo* m_tinfo;
	int64_t m_main_thread_pid;

	// Flags word used by the analysis engine.
	uint16_t m_th_analysis_flags;
	// The analyzer metrics
	sinsp_counters m_metrics; 
	// The transaction metrics
	sinsp_transaction_counters m_transaction_metrics; 
	// The metrics for transaction coming from the external world
	sinsp_transaction_counters m_external_transaction_metrics; 
	// Process-specific information
	sinsp_procinfo* m_procinfo;
	// The ratio between the number of connections waiting to be served and 
	// the total connection queue length for this process.
	uint32_t m_connection_queue_usage_pct;
	// This is used for CPU load calculation
	uint64_t m_old_proc_jiffies;
	// the process CPU load
	double m_cpuload;
	// number of major page at last flush
	uint64_t m_old_pfmajor;
	// number of minor page at last flush
	uint64_t m_old_pfminor;
	// Time and duration of the last select, poll or epoll
	uint64_t m_last_wait_end_time_ns;
	int64_t m_last_wait_duration_ns;
	// The complext state that needs to be explicitly allocated because placement
	// new doesn't support it
	thread_analyzer_dyn_state* m_dynstate;
};

///////////////////////////////////////////////////////////////////////////////
// Thread table changes listener
///////////////////////////////////////////////////////////////////////////////
class analyzer_threadtable_listener : public sinsp_threadtable_listener
{
public:
	analyzer_threadtable_listener(sinsp* inspector, sinsp_analyzer* analyzer);
	void on_thread_created(sinsp_threadinfo* tinfo);
	void on_thread_destroyed(sinsp_threadinfo* tinfo);

private:
	sinsp* m_inspector; 
	sinsp_analyzer* m_analyzer;
};

///////////////////////////////////////////////////////////////////////////////
// Support for thread sorting
///////////////////////////////////////////////////////////////////////////////
bool threadinfo_cmp_cpu(sinsp_threadinfo* src , sinsp_threadinfo* dst); 
bool threadinfo_cmp_memory(sinsp_threadinfo* src , sinsp_threadinfo* dst);
bool threadinfo_cmp_io(sinsp_threadinfo* src , sinsp_threadinfo* dst);
bool threadinfo_cmp_net(sinsp_threadinfo* src , sinsp_threadinfo* dst);
bool threadinfo_cmp_transactions(sinsp_threadinfo* src , sinsp_threadinfo* dst);

bool threadinfo_cmp_cpu_cs(sinsp_threadinfo* src , sinsp_threadinfo* dst); 
bool threadinfo_cmp_memory_cs(sinsp_threadinfo* src , sinsp_threadinfo* dst);
bool threadinfo_cmp_io_cs(sinsp_threadinfo* src , sinsp_threadinfo* dst);
bool threadinfo_cmp_net_cs(sinsp_threadinfo* src , sinsp_threadinfo* dst);
bool threadinfo_cmp_transactions_cs(sinsp_threadinfo* src , sinsp_threadinfo* dst);

#endif // HAS_ANALYZER
