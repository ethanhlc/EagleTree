/*
 * Operating_System.h
 *
 *  Created on: Feb 19, 2013
 *      Author: niv
 */

#ifndef OPERATING_SYSTEM_H_
#define OPERATING_SYSTEM_H_

#include "ssd.h"
#include "scheduler.h"

namespace ssd {

class Thread
{
public:
	Thread();
	virtual ~Thread();
	deque<Event*> run();
	bool is_finished();
	inline void set_time(double current_time) { time = current_time; }
	inline double get_time() { return time; }
	inline void add_follow_up_thread(Thread* thread) { threads_to_start_when_this_thread_finishes.push_back(thread); }
	inline void add_follow_up_threads(vector<Thread*> threads) { threads_to_start_when_this_thread_finishes.insert(threads_to_start_when_this_thread_finishes.end(), threads.begin(), threads.end()); }
	inline vector<Thread*>& get_follow_up_threads() { return threads_to_start_when_this_thread_finishes; }
	virtual void print_thread_stats();
	deque<Event*> register_event_completion(Event* event);
	inline void set_experiment_thread(bool val) { experiment_thread = val; }
	inline bool is_experiment_thread() { return experiment_thread; }
	void set_os(OperatingSystem*  op_sys);
	StatisticsGatherer* get_statistics_gatherer() { return statistics_gatherer; }
	inline void set_statistics_gatherer(StatisticsGatherer* new_statistics_gatherer) { statistics_gatherer = new_statistics_gatherer; }
	inline void set_time_to_wait_before_starting(int time) { time_to_wait_before_starting = time; }
	void init();
protected:
	virtual Event* issue_next_io() = 0;
	virtual void handle_event_completion(Event* event) = 0;
	bool finished;
	double time;
	vector<Thread*> threads_to_start_when_this_thread_finishes;
	OperatingSystem* os;
	void submit(Event* event);
private:
	ulong num_ios_finished;
	bool experiment_thread;
	StatisticsGatherer* statistics_gatherer;
	deque<Event*> submitted_events;
	bool last_IO_was_null;
	int num_IOs_executing;
	int time_to_wait_before_starting;
};


class IO_Pattern_Generator
{
public:
	IO_Pattern_Generator(long min_LBA, long max_LBA) : min_LBA(min_LBA), max_LBA(max_LBA) {};
	virtual ~IO_Pattern_Generator() {};
	virtual int next() = 0;
	const long min_LBA, max_LBA;
};

class Random_IO_Pattern_Generator : public IO_Pattern_Generator
{
public:
	Random_IO_Pattern_Generator(long min_LBA, long max_LBA, ulong seed) : IO_Pattern_Generator(min_LBA, max_LBA), random_number_generator(seed) {};
	~Random_IO_Pattern_Generator() {};
	int next() { return min_LBA + random_number_generator() % (max_LBA - min_LBA + 1); };
private:
	MTRand_int32 random_number_generator;
};

class Sequential_IO_Pattern_Generator : public IO_Pattern_Generator
{
public:
	Sequential_IO_Pattern_Generator(long min_LBA, long max_LBA) : IO_Pattern_Generator(min_LBA, max_LBA), counter(min_LBA - 1) {};
	~Sequential_IO_Pattern_Generator() {};
	int next() { return counter == max_LBA ? counter = min_LBA : ++counter; };
private:
	long counter;
};

class Simple_Thread : public Thread
{
public:
	Simple_Thread(IO_Pattern_Generator* generator, int MAX_IOS, event_type type);
	virtual ~Simple_Thread();
	virtual Event* issue_next_io();
	virtual void handle_event_completion(Event* event);
	inline void set_num_ios(ulong num_ios) { number_of_times_to_repeat = num_ios; }
private:
	long number_of_times_to_repeat;
	event_type type;
	int num_ongoing_IOs;
	const int MAX_IOS;
	IO_Pattern_Generator* io_gen;
};

class Synchronous_Random_Writer : public Simple_Thread
{
public:
	Synchronous_Random_Writer(long min_LBA, long max_LBA, ulong randseed)
		: Simple_Thread(new Random_IO_Pattern_Generator(min_LBA, max_LBA, randseed), 1, WRITE) {}
};

class Synchronous_Random_Reader : public Simple_Thread
{
public:
	Synchronous_Random_Reader(long min_LBA, long max_LBA, ulong randseed )
		: Simple_Thread(new Random_IO_Pattern_Generator(min_LBA, max_LBA, randseed), 1, READ) {}
};

class Asynchronous_Random_Writer : public Simple_Thread
{
public:
	Asynchronous_Random_Writer(long min_LBA, long max_LBA, ulong randseed)
		: Simple_Thread(new Random_IO_Pattern_Generator(min_LBA, max_LBA, randseed), std::numeric_limits<int>::max(), WRITE) {}
};

class Synchronous_Sequential_Writer : public Simple_Thread
{
public:
	Synchronous_Sequential_Writer(long min_LBA, long max_LBA )
		: Simple_Thread(new Sequential_IO_Pattern_Generator(min_LBA, max_LBA), 1, WRITE) {}
};

class Asynchronous_Sequential_Writer : public Simple_Thread
{
public:
	Asynchronous_Sequential_Writer(long min_LBA, long max_LBA)
		: Simple_Thread(new Sequential_IO_Pattern_Generator(min_LBA, max_LBA), std::numeric_limits<int>::max(), WRITE) {
	}
};

class Asynchronous_Sequential_Trimmer : public Simple_Thread
{
public:
	Asynchronous_Sequential_Trimmer(long min_LBA, long max_LBA)
		: Simple_Thread(new Sequential_IO_Pattern_Generator(min_LBA, max_LBA), std::numeric_limits<int>::max(), TRIM) {
	}
};

class Synchronous_Sequential_Reader : public Simple_Thread
{
public:
	Synchronous_Sequential_Reader(long min_LBA, long max_LBA )
		: Simple_Thread(new Sequential_IO_Pattern_Generator(min_LBA, max_LBA), 1, READ) {}
};

class Asynchronous_Sequential_Reader : public Simple_Thread
{
public:
	Asynchronous_Sequential_Reader(long min_LBA, long max_LBA )
		: Simple_Thread(new Sequential_IO_Pattern_Generator(min_LBA, max_LBA), std::numeric_limits<int>::max(), READ) {}
};

class Asynchronous_Random_Thread_Reader_Writer : public Thread
{
public:
	Asynchronous_Random_Thread_Reader_Writer(long min_LBA, long max_LAB, int number_of_times_to_repeat, ulong randseed = 0);
	Event* issue_next_io();
	void handle_event_completion(Event* event) {};
private:
	long min_LBA, max_LBA;
	int number_of_times_to_repeat;
	MTRand_int32 random_number_generator;
};

class Collision_Free_Asynchronous_Random_Thread : public Thread
{
public:
	Collision_Free_Asynchronous_Random_Thread(long min_LBA, long max_LAB, int number_of_times_to_repeat, ulong randseed = 0, event_type type = WRITE);
	Event* issue_next_io();
	void handle_event_completion(Event* event);
private:
	long min_LBA, max_LBA;
	int number_of_times_to_repeat;
	MTRand_int32 random_number_generator;
	event_type type;
	set<long> logical_addresses_submitted;
};

// assuming the relation is made of contigouse pages
// RAM_available is the number of pages that fit into RAM
class External_Sort : public Thread
{
public:
	External_Sort(long relation_min_LBA, long relation_max_LBA, long RAM_available,
			long free_space_min_LBA, long free_space_max_LBA);
	Event* issue_next_io();
	Event* execute_first_phase();
	Event* execute_second_phase();
	Event* execute_third_phase();
	void handle_event_completion(Event* event);
private:
	long relation_min_LBA, relation_max_LBA, RAM_available, free_space_min_LBA, free_space_max_LBA, cursor, counter, number_finished;
	int num_partitions, num_pages_in_last_partition;
	enum external_sort_phase {FIRST_PHASE_READ, FIRST_PHASE_WRITE, SECOND_PHASE, THIRD_PHASE, FINISHED};
	external_sort_phase phase;
	bool can_start_next_read;
};

// Simulates the IO pattern of a grace hash join between two relations
class Grace_Hash_Join : public Thread
{
public:
	Grace_Hash_Join(long relation_A_min_LBA,        long relation_A_max_LBA,
					long relation_B_min_LBA,        long relation_B_max_LBA,
					long free_space_min_LBA,        long free_space_max_LBA,
					bool use_flexible_reads = true, bool use_tagging  = true,
					long rows_per_page      = 32,    int ranseed = 72);
	Event* issue_next_io();

	void handle_event_completion(Event* event);
	void static initialize_counter() { printf("grace_counter: %d\n", grace_counter); grace_counter = 0; };
	int get_counter() { return grace_counter; };
private:
	static int grace_counter;
	void execute_build_phase();

	void execute_probe_phase();
	void setup_probe_run();
	void read_smaller_bucket();
	void trim_smaller_bucket();
	void read_next_in_larger_bucket();

	void create_flexible_reader(int start, int finish);
	void handle_read_completion_build();
	void flush_buffer(int buffer_id);

	long relation_A_min_LBA, relation_A_max_LBA;
	long relation_B_min_LBA, relation_B_max_LBA;
	long free_space_min_LBA, free_space_max_LBA;
	bool use_flexible_reads, use_tagging;
	long input_cursor;
	long relation_A_size, relation_B_size, free_space_size;
	long rows_per_page;
	int num_partitions, partition_size;

	// Bookkeeping variables
	Flexible_Reader* flex_reader;
	MTRand_int32 random_number_generator;
	enum {BUILD, PROBE_SYNCH, PROBE_ASYNCH, DONE} phase;
	vector<int> output_buffers;
	vector<int> output_cursors;
	vector<int> output_cursors_startpoints;
	vector<int> output_cursors_splitpoints; // Separates relation A and B in output buffers
	int victim_buffer;
	int writes_in_progress, reads_in_progress;
	set<long> reads_in_progress_set;
	int small_bucket_begin, small_bucket_end;
	int large_bucket_begin, large_bucket_cursor, large_bucket_end;
	bool finished_reading_smaller_bucket, finished_trimming_smaller_bucket;

};

struct Address_Range {
	long min;
	long max;
	Address_Range(long min, long max);
	long get_size() { return max - min + 1; }
	Address_Range split(long num_pages_in_new_part) {
		Address_Range new_range(min, min + num_pages_in_new_part - 1);
		min += num_pages_in_new_part;
		return new_range;
	}
	bool is_contiguously_followed_by(Address_Range other) const;
	bool is_followed_by(Address_Range other) const;
	void merge(Address_Range other);
};

// This is a file manager that writes one file at a time sequentially
// files might be fragmented across logical space
// files have a random size determined by the file manager
// fragmentation will eventually happen
class File_Manager : public Thread
{
public:
	File_Manager(long min_LBA, long max_LBA, uint num_files_to_write, long max_file_size, ulong randseed = 0);
	~File_Manager();
	Event* issue_next_io();
	void handle_event_completion(Event* event);
	//virtual void print_thread_stats();
	static void reset_id_generator();
private:
	struct File {
		const double death_probability;
		double time_created, time_finished_writing, time_deleted;
		static int file_id_generator;
		const uint size;
		int id;
		uint num_pages_written;
		deque<Address_Range > ranges_comprising_file;
		Address_Range current_range_being_written;
		set<long> logical_addresses_to_be_written_in_current_range;
		uint num_pages_allocated_so_far;

		File(uint size, double death_probability, double time_created);

		long get_num_pages_left_to_allocate() const;
		bool needs_new_range() const;
		bool is_finished() const;
		long get_next_lba_to_be_written();
		void register_new_range(Address_Range range);
		void register_write_completion();
		void finish(double time_finished);
		bool has_issued_last_io();
	};

	void schedule_to_trim_file(File* file);
	double generate_death_probability();
	void write_next_file(double time);
	void assign_new_range();
	void randomly_delete_files(double current_time);
	Event* issue_trim();
	Event* issue_write();
	void reclaim_file_space(File* file);
	void delete_file(File* victim, double current_time);
	void handle_file_completion(double current_time);

	long num_free_pages;
	deque<Address_Range> free_ranges;
	vector<File*> live_files;
	vector<File*> files_history;
	File* current_file;
	long min_LBA, max_LBA;
	int num_files_to_write;

	//Reliable_Random_Int_Generator random_number_generator;
	//Reliable_Random_Double_Generator double_generator;

	MTRand_int32 random_number_generator;
	MTRand_open double_generator;
	set<long> addresses_to_trim;
	const long max_file_size;
	//Throughput_Moderator throughout_moderator;
};

/*struct os_event {
	int thread_id;
	Event* pending_event;
	os_event(int thread_id, Event* event) : thread_id(thread_id), pending_event(event) {}
	os_event() : thread_id(UNDEFINED), pending_event(NULL) {}
};*/

class Flexible_Reader {
public:
	Flexible_Reader(FtlParent const& ftl, vector<Address_Range>);
	Event* read_next(double start_time);
	void register_read_commencement(Flexible_Read_Event*);
	inline bool is_finished() { return finished_counter == 0; }
	inline uint get_num_reads_left() { return finished_counter; }
	inline vector<vector<Address> > const& get_immediate_candidates()     { return immediate_candidates_physical_addresses; }
	inline vector<vector<long> >    const& get_immediate_candidates_lba() { return immediate_candidates_logical_addresses; }
	void find_alternative_immediate_candidate(uint package, uint die);

	//inline vector<vector<long> > const& get_immediate_candidates_logical_addresses() { return immediate_candidates_logical_addresses; }
private:
	void set_new_candidate();
	struct progress_tracker {
		vector<Address_Range> ranges;
		uint current_range;
		uint offset_in_range;
		vector<vector<bool> > completion_bitmap;
		inline bool finished() const { return current_range >= ranges.size(); }
		inline long get_next_lba();
		progress_tracker(vector<Address_Range> ranges);
	};
	progress_tracker pt;

	struct candidate {
		long logical_address;
		Address physical_address;
		candidate(Address phys, long log) : logical_address(log), physical_address(phys) {}
	};
	vector<vector<vector<candidate> > > candidate_list;	// current physcial addresses, ready to be read.

	vector<vector<Address> > immediate_candidates_physical_addresses;
	vector<vector<long> > immediate_candidates_logical_addresses;

	uint finished_counter;
	FtlParent const& ftl;
};

class Flexible_Read_Event : public Event {
private:
	Flexible_Reader* reader;
public:
	Flexible_Read_Event(Flexible_Reader* fr, double time) : Event(READ, 0, 1, time), reader(fr) {};
	~Flexible_Read_Event() {}
	inline vector<vector<Address> > const& get_candidates()     { return reader->get_immediate_candidates(); }
	inline vector<vector<long> >    const& get_candidates_lba() { return reader->get_immediate_candidates_lba(); }
	inline void register_read_commencement() { reader->register_read_commencement(this); }
	inline void find_alternative_immediate_candidate(uint package, uint die) { reader->find_alternative_immediate_candidate(package, die); }
};

class Flexible_Reader_Thread : public Thread
{
public:
	Flexible_Reader_Thread(long min_LBA, long max_LAB, int number_of_times_to_repeat);
	Event* issue_next_io();
	void handle_event_completion(Event* event);
private:
	long min_LBA, max_LBA;
	bool ready_to_issue_next_write;
	int number_of_times_to_repeat;
	Flexible_Reader* flex_reader;
};

class OperatingSystem
{
public:
	OperatingSystem(vector<Thread*> threads);
	~OperatingSystem();
	void run();
	void register_event_completion(Event* event);
	void set_num_writes_to_stop_after(long num_writes);
	double get_experiment_runtime() const;
	Flexible_Reader* create_flexible_reader(vector<Address_Range>);
	void submit(Event* event);
private:
	int pick_unlocked_event_with_shortest_start_time();
	void dispatch_event(int thread_id);
	double get_event_minimal_completion_time(Event const*const event) const;
	bool is_LBA_locked(ulong lba);
	void update_thread_times(double time);
	Ssd * ssd;
	vector<Thread*> threads;

	struct Pending_Events {
		vector<deque<Event*> > event_queues;
		int num_pending_events;
		Pending_Events(int i);
		~Pending_Events();
		Event* peek(int i);
		Event* pop(int i);
		void append(int i, deque<Event*>);
		void push_back() { event_queues.push_back(deque<Event*>()); }
		inline int get_num_pending_events() { return num_pending_events; }
		inline int size() {return event_queues.size();};
	};
	Pending_Events events;

	map<long, queue<uint> > write_LBA_to_thread_id;
	map<long, queue<uint> > read_LBA_to_thread_id;
	map<long, queue<uint> > trim_LBA_to_thread_id;
	void lock(Event* event, int thread_id);
	void release_lock(Event*);

	// used to record which thread dispatched which IO
	map<long, long> app_id_to_thread_id_mapping;

	int currently_executing_ios_counter;
	double last_dispatched_event_minimal_finish_time;

	set<uint> currently_executing_ios;
	long NUM_WRITES_TO_STOP_AFTER;
	long num_writes_completed;

	double time_of_experiment_start;
	double time_of_last_event_completed;

	int counter_for_user;
	int idle_time;
	double time;
};

}


#endif /* OPERATING_SYSTEM_H_ */