/*
 * osmo-fl2k, turns FL2000-based USB 3.0 to VGA adapters into
 * low cost DACs
 * 
 * On Ubuntu: sudo sh -c 'echo 0 > /sys/module/usbcore/parameters/usbfs_memory_mb'
 *
 */

#include <getopt.h>
#include <iostream>
#include <signal.h>
#include <math.h>
#include <map>
#include <vector>
#include <string>
#include <algorithm>
#include <chrono>
#include <thread>

#include "osmo-fl2k.h"
#include "wspr.h"

#define FREQ_BASE 14095600
#define FREQ_SHIFT 1.46
#define SYMBOL_LENGTH_MS 683

#define SIGNAL_MAX 127
#define SAMPLE_RATE  112857136

using namespace std;

int8_t *tx_buffer = nullptr;
vector<int8_t*> tx_symbols;

fl2k_dev_t *fl2k_dev = nullptr;
uint32_t fl2k_dev_idx = 0;

// Catches ^C and stops
static void sighandler(int signum) {
	cout << "Signal caught, exiting!" << endl;
	
	fl2k_stop_tx(fl2k_dev);
	fl2k_close(fl2k_dev);

	exit(0);
}

// USB calls back to get the next buffer of data
void fl2k_callback(fl2k_data_info_t *data_info) {
	if (data_info->device_error) {
		cout << "WARNING: device error" << endl;
	}

	data_info->sampletype_signed = 1;
	data_info->r_buf = (char *)tx_buffer;
}

void init_txbuffer() {
	tx_symbols.push_back((int8_t*)malloc(FL2K_BUF_LEN));
	double c_0 = (2*M_PI * (double)FREQ_BASE) / (double)SAMPLE_RATE;

	tx_symbols.push_back((int8_t*)malloc(FL2K_BUF_LEN));
	double c_1 = (2*M_PI * ((double)FREQ_BASE + 1*FREQ_SHIFT)) / (double)SAMPLE_RATE;

	tx_symbols.push_back((int8_t*)malloc(FL2K_BUF_LEN));
	double c_2 = (2*M_PI * ((double)FREQ_BASE + 2*FREQ_SHIFT)) / (double)SAMPLE_RATE;

	tx_symbols.push_back((int8_t*)malloc(FL2K_BUF_LEN));
	double c_3 = (2*M_PI * ((double)FREQ_BASE + 3*FREQ_SHIFT)) / (double)SAMPLE_RATE;

	for(int i = 0; i < FL2K_BUF_LEN; i++) {
		tx_symbols[0][i] = (int8_t)(sin(c_0 * i) * SIGNAL_MAX);
		tx_symbols[1][i] = (int8_t)(sin(c_1 * i) * SIGNAL_MAX);
		tx_symbols[2][i] = (int8_t)(sin(c_2 * i) * SIGNAL_MAX);
		tx_symbols[3][i] = (int8_t)(sin(c_3 * i) * SIGNAL_MAX);
	}
}

void attach_sighandlers() {
	struct sigaction sigact, sigign;
	
 	sigact.sa_handler = sighandler;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;
	sigign.sa_handler = SIG_IGN;
	sigaction(SIGINT, &sigact, nullptr);
	sigaction(SIGTERM, &sigact, nullptr);
	sigaction(SIGQUIT, &sigact, nullptr);
	sigaction(SIGPIPE, &sigign, nullptr);
}

void set_freq_to_symbol(int symbol) {
	tx_buffer = tx_symbols[symbol];
	cout << symbol << flush;
}


int main(int argc, char **argv) {
	string callsign = "";
	string location = "";
	int power = 0;

	int c = 0;
	while ((c = getopt(argc, argv,"c:l:p:")) != -1) {
		switch (c) {
		case 'c':
			callsign = optarg;
			break;
		case 'l':
			location = optarg; 
			break;
		case 'p':
			power = atoi(optarg);
			break;
		default:
			cout << "Usage: " << argv[0] << " -cCALLSIGN -lLOCATION -pPOWER" << endl;
			exit(EXIT_FAILURE);
		}
	}

	if(callsign.size() == 0 || location.size() == 0 || power == 0) {
		cout << "Usage: " << argv[0] << " -cCALLSIGN -lLOCATION -pPOWER" << endl;
		exit(EXIT_FAILURE);
	}

	transform(callsign.begin(), callsign.end(),callsign.begin(), ::toupper);
	transform(location.begin(), location.end(),location.begin(), ::toupper);

	WsprMessage wspr_msg(callsign.c_str(), location.c_str(), power);
	/*
	cout << "{" << flush;
	for(int i = 0; i < WSPR_MSG_SIZE; i++) {
		cout << (int)wspr_msg.symbols[i] << "," << flush;
	}
	cout << "}" << endl;
	*/

	attach_sighandlers();
	init_txbuffer();
	fl2k_open(&fl2k_dev, fl2k_dev_idx);
	
	if(!fl2k_dev) {
		cout << "Failed to open fl2k device #" << fl2k_dev_idx << endl;
	}
	else {
		cout << "Opened device" << endl;

		int r = fl2k_start_tx(fl2k_dev, fl2k_callback, nullptr, 0);
		
		// Set the sample rate
		r = fl2k_set_sample_rate(fl2k_dev, SAMPLE_RATE);
		if (r < 0) {
			cout << "WARNING: Failed to set sample rate. " << r << endl;
		}
		
		for(int s = 0; s < WSPR_MSG_SIZE; s++) {
			set_freq_to_symbol((int)wspr_msg.symbols[s]);
			this_thread::sleep_for(std::chrono::milliseconds(SYMBOL_LENGTH_MS));
		}
	}

	fl2k_stop_tx(fl2k_dev);
	fl2k_close(fl2k_dev);

	return 0;
}
