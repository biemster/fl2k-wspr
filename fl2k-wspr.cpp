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

#include "osmo-fl2k.h"
#include "wspr.h"

#define SIGNAL_MAX 99			// 99 * 0.70710678 = 70.00.... keeps rounding error in signal to minimum
								// 17 * 0.70710678 = 12.02.... keeps rounding error low and low signal keeps rest of the spectrum relatively clean
#define SIGNAL_MIN -SIGNAL_MAX
#define SF_RATIO 8				// {2,4,8} samplerate / signal frequency ratio (max sample rate is about 140 MS/s on USB3)

#define SIN_1_4_PI 0.70710678		// sin(1/4*pi)

using namespace std;

int8_t *tx_buffer = nullptr;

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
	int8_t sin14pi = (int8_t)(SIGNAL_MAX * SIN_1_4_PI);
	map<int,vector<int8_t>> sines;
	sines[2] = {SIGNAL_MIN, SIGNAL_MAX};
	sines[4] = {0, SIGNAL_MIN, 0, SIGNAL_MAX};
	sines[8] = {0, (int8_t)-sin14pi, SIGNAL_MIN, (int8_t)-sin14pi, 0, sin14pi, SIGNAL_MAX, sin14pi};

	tx_buffer = (int8_t*)malloc(FL2K_BUF_LEN);
	auto sine = sines[SF_RATIO];
	for(int i = 0; i < FL2K_BUF_LEN; i++) {
		tx_buffer[i] = sine[i % SF_RATIO];
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

void set_freq_carrier(uint32_t freq) {
	uint32_t samplerate = freq * SF_RATIO;

	// Set the sample rate
	int r = fl2k_set_sample_rate(fl2k_dev, samplerate);
	if (r < 0) {
		cout << "WARNING: Failed to set sample rate. " << r << endl;
	}

	/* read back actual frequency */
	samplerate = fl2k_get_sample_rate(fl2k_dev);
	cout << "Actual {sample rate,frequency} = {" << samplerate << "," << samplerate/SF_RATIO << "}" << endl;
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

		/* TODO: run timers here to keep transmitting */
	}

	fl2k_stop_tx(fl2k_dev);
	fl2k_close(fl2k_dev);

	return 0;
}
