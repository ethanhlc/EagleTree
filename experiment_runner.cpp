#define BOOST_FILESYSTEM_VERSION 3
#include "ssd.h"
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <stdio.h>  /* defines FILENAME_MAX */
#include <unistd.h>   // chdir
#include <sys/stat.h> // mkdir
#include <iostream>
//#include <boost/filesystem.hpp>

#define SIZE 2

using namespace ssd;

const string Experiment_Runner::markers[] = {"circle", "square", "triangle", "diamond", "cross", "plus", "star", "star2", "star3", "star4", "flower"};

const bool Experiment_Runner::REMOVE_GLE_SCRIPTS_AGAIN = false;

const double Experiment_Runner::M = 1000000.0; // One million
const double Experiment_Runner::K = 1000.0;    // One thousand

double Experiment_Runner::calibration_precision      = 1.0; // microseconds
double Experiment_Runner::calibration_starting_point = 15.00; // microseconds
string Experiment_Runner::base_folder = get_current_dir_name();

void Experiment_Runner::unify_under_one_statistics_gatherer(vector<Thread*> threads, StatisticsGatherer* statistics_gatherer) {
	for (uint i = 0; i < threads.size(); ++i) {
		threads[i]->set_statistics_gatherer(statistics_gatherer);
		unify_under_one_statistics_gatherer(threads[i]->get_follow_up_threads(), statistics_gatherer); // Recurse
	}
}

void Experiment_Runner::run_single_measurment(Workload_Definition* experiment_workload, int IO_limit, OperatingSystem* os) {
	Queue_Length_Statistics::init();
	vector<Thread*> experiment_threads = experiment_workload->generate_instance();
	os->set_threads(experiment_threads);
	os->set_num_writes_to_stop_after(IO_limit);
	os->run();

	StatisticsGatherer::get_global_instance()->print();
	//StatisticsGatherer::get_global_instance()->print_gc_info();
	Utilization_Meter::print();
	//Individual_Threads_Statistics::print();
	//Queue_Length_Statistics::print_distribution();
	Queue_Length_Statistics::print_avg();
	//Free_Space_Meter::print();
	//Free_Space_Per_LUN_Meter::print();
}

vector<Experiment_Result> Experiment_Runner::simple_experiment(Workload_Definition* experiment_workload, string name, long IO_limit, double& variable, double min_val, double max_val, double incr) {
	assert(experiment_workload != NULL);
	string data_folder = base_folder + name + "/";
	mkdir(data_folder.c_str(), 0755);
	Experiment_Result global_result(name, data_folder, "Global/", "Changing a Var");
	global_result.start_experiment();

	for (variable = min_val; variable <= max_val; variable += incr) {
		printf("----------------------------------------------------------------------------------------------------------\n");
		printf("%s :  %f \n", name.c_str(), variable);
		printf("----------------------------------------------------------------------------------------------------------\n");

		string point_folder_name = data_folder + to_string(variable) + "/";
		mkdir(point_folder_name.c_str(), 0755);
		VisualTracer::init(point_folder_name);
		write_config_file(point_folder_name);
		//OperatingSystem* os = load_state();
		OperatingSystem* os = new OperatingSystem();
		run_single_measurment(experiment_workload, IO_limit, os);
		global_result.collect_stats(variable, os->get_experiment_runtime(), StatisticsGatherer::get_global_instance());
		write_results_file(point_folder_name);
		delete os;
	}
	global_result.end_experiment();
	vector<Experiment_Result> results;
	results.push_back(global_result);
	return results;
}

vector<Experiment_Result> Experiment_Runner::simple_experiment(Workload_Definition* experiment_workload, string name, long IO_limit, long& variable, long min_val, long max_val, long incr) {
	assert(experiment_workload != NULL);
	string data_folder = base_folder + name;
	mkdir(data_folder.c_str(), 0755);
	Experiment_Result global_result(name, data_folder, "Global/", "Changing a Var");
	global_result.start_experiment();
	assert(incr > 0);
	for (variable = min_val; variable <= max_val; variable += incr) {
		printf("----------------------------------------------------------------------------------------------------------\n");
		printf("%s :  %d \n", name.c_str(), variable);
		printf("----------------------------------------------------------------------------------------------------------\n");

		OperatingSystem* os = load_state();
		run_single_measurment(experiment_workload, IO_limit, os);
		global_result.collect_stats(variable, os->get_experiment_runtime(), StatisticsGatherer::get_global_instance());
		delete os;
	}
	global_result.end_experiment();
	vector<Experiment_Result> results;
	results.push_back(global_result);
	return results;
}

vector<Experiment_Result> Experiment_Runner::random_writes_on_the_side_experiment(Workload_Definition* workload, int write_threads_min, int write_threads_max, int write_threads_inc, string name, int IO_limit, double used_space, int random_writes_min_lba, int random_writes_max_lba) {
	string data_folder = base_folder + name;
	mkdir(data_folder.c_str(), 0755);
	Experiment_Result global_result       (name, data_folder, "Global/",             "Number of concurrent random write threads");
    Experiment_Result experiment_result   (name, data_folder, "Experiment_Threads/", "Number of concurrent random write threads");
    Experiment_Result write_threads_result(name, data_folder, "Noise_Threads/",      "Number of concurrent random write threads");

    global_result.start_experiment();
    experiment_result.start_experiment();
    write_threads_result.start_experiment();

    for (int random_write_threads = write_threads_min; random_write_threads <= write_threads_max; random_write_threads += write_threads_inc) {
		printf("----------------------------------------------------------------------------------------------------------\n");
		printf("%s : Experiment with max %d concurrent random writes threads.\n", name.c_str(), random_write_threads);
		printf("----------------------------------------------------------------------------------------------------------\n");

		StatisticsGatherer* experiment_statistics_gatherer = new StatisticsGatherer();
		StatisticsGatherer* random_writes_statics_gatherer = new StatisticsGatherer();
		Thread* initial_write    = new Asynchronous_Sequential_Writer(0, used_space);
		if (workload != NULL) {
			vector<Thread*> experiment_threads = workload->generate_instance();
			unify_under_one_statistics_gatherer(experiment_threads, experiment_statistics_gatherer);
			initial_write->add_follow_up_threads(experiment_threads);
		}
		for (int i = 0; i < random_write_threads; i++) {
			ulong randseed = (i*3)+537;
			Simple_Thread* random_writes = new Synchronous_Random_Writer(random_writes_min_lba, random_writes_max_lba, randseed);
			Simple_Thread* random_reads = new Synchronous_Random_Reader(random_writes_min_lba, random_writes_max_lba, randseed+461);
			/*if (workload == NULL) {
				random_writes->set_experiment_thread(true);
				random_reads->set_experiment_thread(true);
			}*/
			random_writes->set_num_ios(INFINITE);
			random_reads->set_num_ios(INFINITE);
			random_writes->set_statistics_gatherer(random_writes_statics_gatherer);
			random_reads->set_statistics_gatherer(random_writes_statics_gatherer);
			initial_write->add_follow_up_thread(random_writes);
			initial_write->add_follow_up_thread(random_reads);
		}

		vector<Thread*> threads;
		threads.push_back(initial_write);
		OperatingSystem* os = new OperatingSystem();
		os->set_threads(threads);
		os->set_num_writes_to_stop_after(IO_limit);
		os->run();

			// Collect statistics from this experiment iteration (save in csv files)
		global_result.collect_stats       (random_write_threads, os->get_experiment_runtime(), StatisticsGatherer::get_global_instance());
		experiment_result.collect_stats   (random_write_threads, os->get_experiment_runtime(), experiment_statistics_gatherer);
		write_threads_result.collect_stats(random_write_threads, os->get_experiment_runtime(), random_writes_statics_gatherer);

		if (workload == NULL) {
			StatisticsGatherer::get_global_instance()->print();
		} else {
			experiment_statistics_gatherer->print();
		}

		//StatisticsGatherer::get_global_instance()->print();
		//random_writes_statics_gatherer->print();
		if (PRINT_LEVEL >= 1) {
			StateVisualiser::print_page_status();
			StateVisualiser::print_block_ages();
		}
		delete os;
		delete experiment_statistics_gatherer;
		delete random_writes_statics_gatherer;
	}
    global_result.end_experiment();
    experiment_result.end_experiment();
    write_threads_result.end_experiment();

    vector<Experiment_Result> results;
    results.push_back(global_result);
    results.push_back(experiment_result);
    results.push_back(write_threads_result);
    if (workload != NULL)
    	delete workload;
    return results;
}

Experiment_Result Experiment_Runner::copyback_experiment(vector<Thread*> (*experiment)(int highest_lba), int used_space, int max_copybacks, string data_folder, string name, int IO_limit) {
    Experiment_Result experiment_result(name, data_folder, "", "CopyBacks allowed before ECC check");
    experiment_result.start_experiment();

    const int num_pages = NUMBER_OF_ADDRESSABLE_BLOCKS() * BLOCK_SIZE;
    for (int copybacks_allowed = 0; copybacks_allowed <= max_copybacks; copybacks_allowed += 1) {
		int highest_lba = (int) ((double) num_pages * used_space / 100);
		printf("---------------------------------------\n");
		printf("Experiment with %d copybacks allowed.\n", copybacks_allowed);
		printf("---------------------------------------\n");

		MAX_REPEATED_COPY_BACKS_ALLOWED = copybacks_allowed;

		// Run experiment
		vector<Thread*> threads = experiment(highest_lba);
		OperatingSystem* os = new OperatingSystem();
		os->set_threads(threads);
		os->set_num_writes_to_stop_after(IO_limit);
		os->run();

		// Collect statistics from this experiment iteration (save in csv files)
		experiment_result.collect_stats(copybacks_allowed, os->get_experiment_runtime());

		StatisticsGatherer::get_global_instance()->print();
		if (PRINT_LEVEL >= 1) {
			StateVisualiser::print_page_status();
			StateVisualiser::print_block_ages();
		}

		delete os;
	}

	experiment_result.end_experiment();
	return experiment_result;
}

Experiment_Result Experiment_Runner::copyback_map_experiment(vector<Thread*> (*experiment)(int highest_lba), int cb_map_min, int cb_map_max, int cb_map_inc, int used_space, string data_folder, string name, int IO_limit) {
    Experiment_Result experiment_result(name, data_folder, "", "Max copyback map size");
    experiment_result.start_experiment();

    const int num_pages = NUMBER_OF_ADDRESSABLE_BLOCKS() * BLOCK_SIZE;
    for (int copyback_map_size = cb_map_min; copyback_map_size <= cb_map_max; copyback_map_size += cb_map_inc) {
		int highest_lba = (int) ((double) num_pages * used_space / 100);
		printf("-------------------------------------------------------\n");
		printf("Experiment with %d copybacks allowed in copyback map.  \n", copyback_map_size);
		printf("-------------------------------------------------------\n");

		MAX_ITEMS_IN_COPY_BACK_MAP = copyback_map_size;

		// Run experiment
		vector<Thread*> threads = experiment(highest_lba);
		OperatingSystem* os = new OperatingSystem();
		os->set_threads(threads);
		os->set_num_writes_to_stop_after(IO_limit);
		os->run();

		// Collect statistics from this experiment iteration (save in csv files)
		experiment_result.collect_stats(copyback_map_size, os->get_experiment_runtime());

		StatisticsGatherer::get_global_instance()->print();
		if (PRINT_LEVEL >= 1) {
			StateVisualiser::print_page_status();
			StateVisualiser::print_block_ages();
		}

		delete os;
	}

	experiment_result.end_experiment();
	return experiment_result;
}

// currently, this method checks if there if a file already exists, and if so, assumes it is valid.
// ideally, a check should be made to ensure the saved SSD state matches with the state of the current global parameters
void Experiment_Runner::calibrate_and_save(Workload_Definition* workload, bool force) {
	string file_name = base_folder + "calibrated_state.txt";
	std::ifstream ifile(file_name.c_str());
	if (ifile && !force) {
		return; // file exists
	}
	VisualTracer::init();
	printf("Creating calibrated SSD state.\n");
	OperatingSystem* os = new OperatingSystem();
	os->set_num_writes_to_stop_after(NUMBER_OF_ADDRESSABLE_PAGES() * 2);
	vector<Thread*> init_threads = workload->generate_instance();
	os->set_threads(init_threads);
	os->run();
	os->get_ssd()->execute_all_remaining_events();
	save_state(os, file_name);
	delete os;
}

void Experiment_Runner::write_config_file(string folder_name) {
	string file_name = folder_name + "configuration.txt";
	FILE* file = fopen(file_name.c_str() , "w");
	print_config(file);
	fclose(file);
}

void Experiment_Runner::write_results_file(string folder_name) {
	string file_name = folder_name + "results.txt";
	FILE* file = fopen(file_name.c_str() , "w");
	StatisticsGatherer::get_global_instance()->print_simple(file);
	double channel_util = Utilization_Meter::get_avg_channel_utilization();
	fprintf(file, "channel util:\t%f\n", channel_util);
	double LUN_util = Utilization_Meter::get_avg_LUN_utilization();
	fprintf(file, "LUN util:\t%f\n", LUN_util);
}

void Experiment_Runner::save_state(OperatingSystem* os, string file_name) {
	std::ofstream file(file_name.c_str());
	boost::archive::text_oarchive oa(file);
	oa.register_type<FtlImpl_Page>( );
	oa.register_type<Block_manager_parallel>( );
	oa.register_type<Sequential_Locality_BM>( );
	oa << os;
}

OperatingSystem* Experiment_Runner::load_state() {
	string file_name = base_folder + "calibrated_state.txt";
	std::ifstream file(file_name.c_str());
	boost::archive::text_iarchive ia(file);
	ia.register_type<FtlImpl_Page>( );
	ia.register_type<Block_manager_parallel>();
	ia.register_type<Sequential_Locality_BM>( );
	OperatingSystem* os;
	ia >> os;

	IOScheduler* scheduler = os->get_ssd()->get_scheduler();
	scheduler->init();
	Block_manager_parent* bm = Block_manager_parent::get_new_instance();
	bm->copy_state(scheduler->get_bm());
	delete scheduler->get_bm();
	scheduler->set_block_manager(bm);
	Migrator* m = scheduler->get_migrator();
	m->set_block_manager(bm);
	Garbage_Collector* gc = m->get_garbage_collector();
	gc->set_block_manager(bm);

	return os;
}

void Experiment_Runner::create_base_folder(string name) {
	base_folder = name;
	printf("%s\n", base_folder.c_str());
	mkdir(base_folder.c_str(), 0755);
}
