/*
 * Device support for Ettus Research UHD driver 
 * Written by Tom Tsou <tom@tsou.cc>
 *
 * Copyright 2010-2011 Free Software Foundation, Inc.
 * Copyright 2014 Ettus Research LLC
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * See the COPYING file in the main directory for details.
 */

#include <uhd/version.hpp>
#include <uhd/property_tree.hpp>
#include <uhd/utils/thread_priority.hpp>
#include <uhd/utils/msg.hpp>

#include "Threads.h"
#include "Logger.h"
#include "UHDDevice.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/*
 * Transmit packet synchronization count
 *
 * During device start and periods of underrun recovery, we need to prevent
 * flooding the device with buffered late packets. This parameter is the number
 * of packets to remove from the beginning of a transmit stream so that we get
 * close to an on-time packet arrival.
 */
#define TX_PACKET_SYNC		30

/*
 * B200/B210 FPGA clocking rate
 *
 * Rate derived from the fixed 100 MHz N200 clocking rate. We use a device
 * sample rate of 6.25 Msps, or approximately 1.63 samples-per-chip based on the
 * 3.84 Mcps UMTS rate. This provides a balance of sufficient filtering and
 * device compatibility.
 */
#define B2XX_CLK_RT		25e6

/*
 * Device clocking offset limit
 *
 * Error if the actual device clocking rate differs from the requested rate by
 * more than this limit.
 */
#define MASTER_CLK_LIMIT	10.0

/* Timestamped ring buffer size */
#define SAMPLE_BUF_SZ		(1 << 20)

struct uhd_dev_offset {
	enum uhd_dev_type type;
	int offset;
};

/*
 * Tx/Rx sample offset values
 *
 * Timing adjustment in samples. This value accounts is empiracally measured
 * and accounts for overall group delay digital filters and analog components.
 */
static struct uhd_dev_offset uhd_offsets[NUM_USRP_TYPES] = {
	{ USRP1, 0 },
	{ USRP2, 61 },
	{ B100,  0 },
	{ B2XX,  99 },
	{ X300,  73 },
	{ UMTRX, 0 },
};

static int get_dev_offset(enum uhd_dev_type type)
{
	if ((type != B2XX) && (type != USRP2) && (type != X300)) {
		LOG(ALERT) << "Unsupported device type";
		return 0;
	}

	for (int i = 0; i < NUM_USRP_TYPES; i++) {
		if (type == uhd_offsets[i].type)
			return uhd_offsets[i].offset;
	}

	return 0;
}

static void *async_event_loop(UHDDevice *dev)
{
	while (1) {
		dev->recv_async_msg();
		pthread_testcancel();
	}

	return NULL;
}

/* 
 * Catch and drop underrun 'U' and overrun 'O' messages from stdout
 * since we already report using the logging facility. Direct
 * everything else appropriately.
 */
void uhd_msg_handler(uhd::msg::type_t type, const std::string &msg)
{
	switch (type) {
	case uhd::msg::status:
		LOG(INFO) << msg;
		break;
	case uhd::msg::warning:
		LOG(WARNING) << msg;
		break;
	case uhd::msg::error:
		LOG(ERR) << msg;
		break;
	case uhd::msg::fastpath:
		break;
	}
}

static void thread_enable_cancel(bool cancel)
{
	cancel ? pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL) :
		 pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
}

UHDDevice::UHDDevice(double rate)
	: tx_gain_min(0.0), tx_gain_max(0.0),
	  rx_gain_min(0.0), rx_gain_max(0.0),
	  tx_rate(rate), rx_rate(rate), tx_freq(0.0), rx_freq(0.0),
	  started(false), aligned(false), rx_pkt_cnt(0), drop_cnt(0),
	  prev_ts(0), ts_offset(0), rx_buffer(NULL)
{
}

UHDDevice::~UHDDevice()
{
	stop();
	delete rx_buffer;
}

void UHDDevice::init_gains()
{
	uhd::gain_range_t range;

	range = usrp_dev->get_tx_gain_range();
	tx_gain_min = range.start();
	tx_gain_max = range.stop();

	range = usrp_dev->get_rx_gain_range();
	rx_gain_min = range.start();
	rx_gain_max = range.stop();

	usrp_dev->set_tx_gain((tx_gain_min + tx_gain_max) / 2);
	usrp_dev->set_rx_gain((rx_gain_min + rx_gain_max) / 2);

	return;
}

int UHDDevice::set_master_clk(double clk_rate)
{
	double actual, offset, limit = 10.0;

	try {
		usrp_dev->set_master_clock_rate(clk_rate);
	} catch (const std::exception &ex) {
		LOG(ALERT) << "UHD clock rate setting failed: " << clk_rate;
		LOG(ALERT) << ex.what();
		return -1;
	}

	actual = usrp_dev->get_master_clock_rate();
	offset = fabs(clk_rate - actual);

	if (offset > limit) {
		LOG(ALERT) << "Failed to set master clock rate";
		LOG(ALERT) << "Requested clock rate " << clk_rate;
		LOG(ALERT) << "Actual clock rate " << actual;
		return -1;
	}

	return 0;
}

int UHDDevice::set_rates(double tx_rate, double rx_rate)
{
	double offset_limit = 1.0;
	double tx_offset, rx_offset;

	/* B2XX is the only device where we set FPGA clocking */
	if (dev_type == B2XX) {
		if (set_master_clk(B2XX_CLK_RT) < 0)
			return -1;
	}

	try {
		usrp_dev->set_tx_rate(tx_rate);
		usrp_dev->set_rx_rate(rx_rate);
	} catch (const std::exception &ex) {
		LOG(ALERT) << "UHD rate setting failed";
		LOG(ALERT) << ex.what();
		return -1;
	}
	this->tx_rate = usrp_dev->get_tx_rate();
	this->rx_rate = usrp_dev->get_rx_rate();

	tx_offset = fabs(this->tx_rate - tx_rate);
	rx_offset = fabs(this->rx_rate - rx_rate);
	if ((tx_offset > offset_limit) || (rx_offset > offset_limit)) {
		LOG(ALERT) << "Actual sample rate differs from desired rate";
		LOG(ALERT) << "Tx/Rx (" << this->tx_rate << "/"
			  << this->rx_rate << ")" << std::endl;
		return -1;
	}

	return 0;
}

double UHDDevice::setTxGain(double db)
{
	usrp_dev->set_tx_gain(db);
	double tx_gain = usrp_dev->get_tx_gain();

	LOG(INFO) << "Set TX gain to " << tx_gain << "dB";

	return tx_gain;
}

double UHDDevice::setRxGain(double db)
{
	usrp_dev->set_rx_gain(db);
	double rx_gain = usrp_dev->get_rx_gain();

	LOG(INFO) << "Set RX gain to " << rx_gain << "dB";

	return rx_gain;
}

/*
 * Parse the UHD device tree and mboard name to find out what device we're
 * dealing with. We need the window type so that the transceiver knows how to
 * deal with the transport latency. Reject the USRP1 because UHD doesn't
 * support timestamped samples with it.
 */
bool UHDDevice::parse_dev_type()
{
	std::string mboard_str, dev_str;
	uhd::property_tree::sptr prop_tree;
	size_t usrp2_str, b200_str, b210_str, x300_str, x310_str;

	prop_tree = usrp_dev->get_device()->get_tree();
	dev_str = prop_tree->access<std::string>("/name").get();
	mboard_str = usrp_dev->get_mboard_name();

	usrp2_str = dev_str.find("USRP2");
	b200_str = mboard_str.find("B200");
	b210_str = mboard_str.find("B210");
	x300_str = mboard_str.find("X300");
	x310_str = mboard_str.find("X310");

	if (b200_str != std::string::npos) {
		dev_type = B2XX;
	} else if (b210_str != std::string::npos) {
		dev_type = B2XX;
	} else if (usrp2_str != std::string::npos) {
		dev_type = USRP2;
	} else if (x300_str != std::string::npos) {
		dev_type = X300;
	} else if (x310_str != std::string::npos) {
		dev_type = X300;
	} else {
		goto nosupport;
	}

	tx_window = TX_WINDOW_FIXED;
	LOG(INFO) << "Using fixed transmit window for "
		  << dev_str << " " << mboard_str;
	return true;

nosupport:
	LOG(ALERT) << "Device not supported by OpenBTS-UMTS";
	return false;
}

bool UHDDevice::open(const std::string &args, bool extref)
{
	/* Find UHD devices */
	uhd::device_addr_t addr(args);
	uhd::device_addrs_t dev_addrs = uhd::device::find(addr);
	if (dev_addrs.size() == 0) {
		LOG(ALERT) << "No UHD devices found with address '" << args << "'";
		return false;
	}

	/* Use the first found device */
	LOG(INFO) << "Using discovered UHD device " << dev_addrs[0].to_string();
	try {
		usrp_dev = uhd::usrp::multi_usrp::make(dev_addrs[0]);
	} catch(...) {
		LOG(ALERT) << "UHD make failed, device " << dev_addrs[0].to_string();
		return false;
	}

	/* Check for a valid device type and set bus type */
	if (!parse_dev_type())
		return false;

	if (extref)
		usrp_dev->set_clock_source("external");

	/* Create TX and RX streamers */
	uhd::stream_args_t stream_args("sc16");
	tx_stream = usrp_dev->get_tx_stream(stream_args);
	rx_stream = usrp_dev->get_rx_stream(stream_args);

	/* Number of samples per over-the-wire packet */
	tx_spp = tx_stream->get_max_num_samps();
	rx_spp = rx_stream->get_max_num_samps();

	set_rates(tx_rate, rx_rate);

	/* Create receive buffer */
	size_t buf_len = SAMPLE_BUF_SZ / sizeof(uint32_t);
	rx_buffer = new SampleBuffer(buf_len, rx_rate);

	/* Set receive chain sample offset */
	ts_offset = get_dev_offset(dev_type);

	/* Initialize and shadow gain values */
	init_gains();

	LOG(INFO) << "\n" << usrp_dev->get_pp_string();

	return true;
}

bool UHDDevice::flush_recv(size_t num_pkts)
{
	uhd::rx_metadata_t md;
	size_t num_smpls, chans = 1;
	float timeout = 0.5f;

	std::vector<std::vector<short> >
		pkt_bufs(chans, std::vector<short>(2 * rx_spp));

	std::vector<short *> pkt_ptrs;
	for (size_t i = 0; i < pkt_bufs.size(); i++)
		pkt_ptrs.push_back(&pkt_bufs[i].front());

	ts_initial = 0;
	while (!ts_initial || (num_pkts-- > 0)) {
		num_smpls = rx_stream->recv(pkt_ptrs, rx_spp, md,
					    timeout, true);
		if (!num_smpls) {
			switch (md.error_code) {
			case uhd::rx_metadata_t::ERROR_CODE_TIMEOUT:
				LOG(ALERT) << "Device timed out";
				return false;
			default:
				continue;
			}
		}

		ts_initial = md.time_spec.to_ticks(rx_rate);
	}

	LOG(INFO) << "Initial timestamp " << ts_initial << std::endl;

	return true;
}

bool UHDDevice::restart()
{
	double delay = 0.1;

	aligned = false;

	uhd::time_spec_t current = usrp_dev->get_time_now();

	uhd::stream_cmd_t cmd = uhd::stream_cmd_t::STREAM_MODE_START_CONTINUOUS;
	cmd.stream_now = false;
	cmd.time_spec = uhd::time_spec_t(current.get_real_secs() + delay);

	usrp_dev->issue_stream_cmd(cmd);

	return flush_recv(10);
}

bool UHDDevice::start()
{
	LOG(INFO) << "Starting USRP...";

	if (started) {
		LOG(ERR) << "Device already running";
		return false;
	}

	setPriority();

	/* Register msg handler */
	uhd::msg::register_handler(&uhd_msg_handler);

	/* Start receive streaming */
	if (!restart())
		return false;

	/* Start asynchronous event (underrun check) loop */
	async_event_thrd = new Thread();
	async_event_thrd->start((void * (*)(void*))async_event_loop, (void*) this);

	/* Display USRP time */
	double time_now = usrp_dev->get_time_now().get_real_secs();
	LOG(INFO) << "The current time is " << time_now << " seconds";

	started = true;
	return true;
}

/*
 * Stop the UHD device
 *
 * Issue a stop streaming command and cancel the asynchronous message loop.
 * Deallocate the asynchronous thread object since we can't reuse it anyways.
 * A new thread object is allocated on device start.
 */
bool UHDDevice::stop()
{
	if (!started) {
		LOG(ERR) << "Device not running";
		return false;
	}

	uhd::stream_cmd_t stream_cmd =
		uhd::stream_cmd_t::STREAM_MODE_STOP_CONTINUOUS;

	usrp_dev->issue_stream_cmd(stream_cmd);

	async_event_thrd->cancel();
	async_event_thrd->join();
	delete async_event_thrd;

	started = false;
	return true;
}

void UHDDevice::setPriority()
{
	uhd::set_thread_priority_safe();
	return;
}

int UHDDevice::check_rx_md_err(uhd::rx_metadata_t &md, ssize_t num_smpls)
{
	long long ts;

	if (!num_smpls) {
		LOG(ERR) << str_code(md);

		switch (md.error_code) {
		case uhd::rx_metadata_t::ERROR_CODE_TIMEOUT:
			LOG(ALERT) << "UHD: Receive timed out";
		case uhd::rx_metadata_t::ERROR_CODE_OVERFLOW:
		case uhd::rx_metadata_t::ERROR_CODE_LATE_COMMAND:
		case uhd::rx_metadata_t::ERROR_CODE_BROKEN_CHAIN:
		case uhd::rx_metadata_t::ERROR_CODE_BAD_PACKET:
		default:
			return ERROR_UNHANDLED;
		}
	}

	/* Missing timestamp */
	if (!md.has_time_spec) {
		LOG(ALERT) << "UHD: Received packet missing timestamp";
		return ERROR_UNRECOVERABLE;
	}

	ts = md.time_spec.to_ticks(rx_rate);

	/* Monotonicity check */
	if (ts < prev_ts) {
		LOG(ALERT) << "UHD: Loss of monotonic time";
		LOG(ALERT) << "Current time: " << ts << ", "
			   << "Previous time: " << prev_ts;
		return ERROR_TIMING;
	} else {
		prev_ts = ts;
	}

	return 0;
}

int UHDDevice::readSamples(short *buf, int len, bool *overrun,
			    long long timestamp, bool *underrun, unsigned *RSSI)
{
	ssize_t rc;
	uhd::time_spec_t timespec;
	uhd::rx_metadata_t metadata;
	uint32_t pkt_buf[rx_spp];

	*overrun = false;
	*underrun = false;

	/* Align read time sample timing with respect to transmit clock */
	timestamp += ts_offset;

	timespec = uhd::time_spec_t::from_ticks(timestamp, rx_rate);
	LOG(DEBUG) << "Requested timestamp = " << timestamp;

	/* Check that timestamp is valid */
	rc = rx_buffer->avail_smpls(timestamp);
	if (rc < 0) {
		LOG(ERR) << rx_buffer->str_code(rc);
		LOG(ERR) << rx_buffer->str_status(timestamp);
		return 0;
	}

	/* Receive samples from the usrp until we have enough */
	while (rx_buffer->avail_smpls(timestamp) < len) {
		thread_enable_cancel(false);
		size_t num_smpls = rx_stream->recv(
					(void *) pkt_buf,
					rx_spp,
					metadata,
					0.1,
					true);
		thread_enable_cancel(true);

		rx_pkt_cnt++;

		/* Check for errors */
		rc = check_rx_md_err(metadata, num_smpls);
		switch (rc) {
		case ERROR_UNRECOVERABLE:
			LOG(ALERT) << "UHD: Version " << uhd::get_version_string();
			LOG(ALERT) << "UHD: Unrecoverable error, exiting...";
			exit(-1);
		case ERROR_TIMING:
		case ERROR_UNHANDLED:
			continue;
		}


		timespec = metadata.time_spec;
		LOG(DEBUG) << "Received timestamp = " << timespec.to_ticks(rx_rate);

		rc = rx_buffer->write(pkt_buf,
				      num_smpls,
				      metadata.time_spec);

		/* Continue on local overrun, exit on other errors */
		if ((rc < 0)) {
			LOG(ERR) << rx_buffer->str_code(rc);
			LOG(ERR) << rx_buffer->str_status(timestamp);
			if (rc != SampleBuffer::ERROR_OVERFLOW)
				return 0;
		}
	}

	/* We have enough samples */
	rc = rx_buffer->read(buf, len, timestamp);
	if ((rc < 0) || (rc != len)) {
		LOG(ERR) << rx_buffer->str_code(rc);
		LOG(ERR) << rx_buffer->str_status(timestamp);
		return 0;
	}

	return len;
}

int UHDDevice::writeSamples(short *buf, int len,
			     bool *underrun, long long ts)
{
	uhd::tx_metadata_t metadata;
	metadata.has_time_spec = true;
	metadata.start_of_burst = false;
	metadata.end_of_burst = false;
	metadata.time_spec = uhd::time_spec_t::from_ticks(ts, tx_rate);

	*underrun = false;

	/* Drop a fixed number of packets */
	if (!aligned) {
		drop_cnt++;

		if (drop_cnt == 1) {
			LOG(DEBUG) << "Aligning transmitter: stop burst";
			*underrun = true;
			metadata.end_of_burst = true;
		} else if (drop_cnt < TX_PACKET_SYNC) {
			LOG(DEBUG) << "Aligning transmitter: packet advance";
			return len;
		} else {
			LOG(DEBUG) << "Aligning transmitter: start burst";
			metadata.start_of_burst = true;
			aligned = true;
			drop_cnt = 0;
		}
	}

	thread_enable_cancel(false);
	size_t num_smpls = tx_stream->send(buf, len, metadata);
	thread_enable_cancel(true);

	if (num_smpls != (unsigned) len) {
		LOG(ALERT) << "UHD: Device send timed out";
	}

	return num_smpls;
}

bool UHDDevice::setTxFreq(double wFreq)
{
	uhd::tune_result_t tr = usrp_dev->set_tx_freq(wFreq);
	LOG(INFO) << "\n" << tr.to_pp_string();
	tx_freq = usrp_dev->get_tx_freq();

	return true;
}

bool UHDDevice::setRxFreq(double wFreq)
{
	uhd::tune_result_t tr = usrp_dev->set_rx_freq(wFreq);
	LOG(INFO) << "\n" << tr.to_pp_string();
	rx_freq = usrp_dev->get_rx_freq();

	return true;
}

bool UHDDevice::recv_async_msg()
{
	uhd::async_metadata_t md;

	thread_enable_cancel(false);
	bool rc = tx_stream->recv_async_msg(md, 0.1);
	thread_enable_cancel(true);
	if (!rc)
		return false;

	/* Assume that any error requires resynchronization */
	if (md.event_code != uhd::async_metadata_t::EVENT_CODE_BURST_ACK) {
		aligned = false;

		if ((md.event_code != uhd::async_metadata_t::EVENT_CODE_UNDERFLOW) &&
		    (md.event_code != uhd::async_metadata_t::EVENT_CODE_TIME_ERROR)) {
			LOG(ERR) << str_code(md);
		}
	}

	return true;
}

std::string UHDDevice::str_code(uhd::rx_metadata_t metadata)
{
	std::ostringstream ost("UHD: ");

	switch (metadata.error_code) {
	case uhd::rx_metadata_t::ERROR_CODE_NONE:
		ost << "No error";
		break;
	case uhd::rx_metadata_t::ERROR_CODE_TIMEOUT:
		ost << "No packet received, implementation timed-out";
		break;
	case uhd::rx_metadata_t::ERROR_CODE_LATE_COMMAND:
		ost << "A stream command was issued in the past";
		break;
	case uhd::rx_metadata_t::ERROR_CODE_BROKEN_CHAIN:
		ost << "Expected another stream command";
		break;
	case uhd::rx_metadata_t::ERROR_CODE_OVERFLOW:
		ost << "An internal receive buffer has filled";
		break;
	case uhd::rx_metadata_t::ERROR_CODE_BAD_PACKET:
		ost << "The packet could not be parsed";
		break;
	default:
		ost << "Unknown error " << metadata.error_code;
	}

	if (metadata.has_time_spec)
		ost << " at " << metadata.time_spec.get_real_secs() << " sec.";

	return ost.str();
}

std::string UHDDevice::str_code(uhd::async_metadata_t metadata)
{
	std::ostringstream ost("UHD: ");

	switch (metadata.event_code) {
	case uhd::async_metadata_t::EVENT_CODE_BURST_ACK:
		ost << "A packet was successfully transmitted";
		break;
	case uhd::async_metadata_t::EVENT_CODE_UNDERFLOW:
		ost << "An internal send buffer has emptied";
		break;
	case uhd::async_metadata_t::EVENT_CODE_SEQ_ERROR:
		ost << "Packet loss between host and device";
		break;
	case uhd::async_metadata_t::EVENT_CODE_TIME_ERROR:
		ost << "Packet time was too late or too early";
		break;
	case uhd::async_metadata_t::EVENT_CODE_UNDERFLOW_IN_PACKET:
		ost << "Underflow occurred inside a packet";
		break;
	case uhd::async_metadata_t::EVENT_CODE_SEQ_ERROR_IN_BURST:
		ost << "Packet loss within a burst";
		break;
	default:
		ost << "Unknown error " << metadata.event_code;
	}

	if (metadata.has_time_spec)
		ost << " at " << metadata.time_spec.get_real_secs() << " sec.";

	return ost.str();
}
