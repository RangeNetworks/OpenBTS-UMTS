/*
 * Timestamped ring buffer implementation 
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

#include <string.h>
#include "SampleBuffer.h"

SampleBuffer::SampleBuffer(int len, double rate)
	: clock_rate(rate), time_start(0), time_end(0),
	  data_start(0), data_end(0)
{
	this->len = len;
	data = new std::complex<short>[len];
}

SampleBuffer::~SampleBuffer()
{
	delete[] data;
}

int SampleBuffer::avail_smpls(long long ts) const
{
	if (ts < time_start)
		return ERROR_TIMESTAMP;
	else if (ts >= time_end)
		return 0;
	else
		return time_end - ts;
}

int SampleBuffer::avail_smpls(uhd::time_spec_t ts) const
{
	return avail_smpls(ts.to_ticks(clock_rate));
}

int SampleBuffer::read(void *buf, int len, long long ts)
{
	int type_size = sizeof(std::complex<short>);

	/* Check for valid read */
	if (ts < time_start)
		return ERROR_TIMESTAMP;
	if (ts >= time_end)
		return 0;
	if (len >= this->len)
		return ERROR_READ;

	/* How many samples should be copied */
	int num_smpls = time_end - ts;
	if (num_smpls > len)
		num_smpls = len;

	/* Starting index */
	int read_start = (data_start + (ts - time_start)) % this->len;

	/* Read it */
	if (read_start + num_smpls < this->len) {
		int numBytes = len * type_size;
		memcpy(buf, data + read_start, numBytes);
	} else {
		int first_cp = (this->len - read_start) * type_size;
		int second_cp = len * type_size - first_cp;

		memcpy(buf, data + read_start, first_cp);
		memcpy((char*) buf + first_cp, data, second_cp);
	}

	data_start = (read_start + len) % this->len;
	time_start = ts + len;

	if (time_start > time_end)
		return ERROR_READ;
	else
		return num_smpls;
}

int SampleBuffer::read(void *buf, int len, uhd::time_spec_t ts)
{
	return read(buf, len, ts.to_ticks(clock_rate));
}

int SampleBuffer::write(void *buf, int len, long long ts)
{
	int type_size = sizeof(std::complex<short>);

	/* Check for valid write */
	if ((len == 0) || (len >= this->len))
		return ERROR_WRITE;
	if ((ts + len) <= time_end)
		return ERROR_TIMESTAMP;

	/* Starting index */
	int write_start = (data_start + (ts - time_start)) % this->len;

	/* Write it */
	if ((write_start + len) < this->len) {
		int numBytes = len * type_size;
		memcpy(data + write_start, buf, numBytes);
	} else {
		int first_cp = (this->len - write_start) * type_size;
		int second_cp = len * type_size - first_cp;

		memcpy(data + write_start, buf, first_cp);
		memcpy(data, (char*) buf + first_cp, second_cp);
	}

	data_end = (write_start + len) % this->len;
	time_end = ts + len;

	if (((write_start + len) > this->len) && (data_end > data_start))
		return ERROR_OVERFLOW;
	else if (time_end <= time_start)
		return ERROR_WRITE;
	else
		return len;
}

int SampleBuffer::write(void *buf, int len, uhd::time_spec_t ts)
{
	return write(buf, len, ts.to_ticks(clock_rate));
}

std::string SampleBuffer::str_status(long long ts) const
{
	std::ostringstream ost("Sample buffer: ");

	ost << "ts = " << ts;
	ost << ", len = " << this->len;
	ost << ", time_start = " << time_start;
	ost << ", time_end = " << time_end;
	ost << ", data_start = " << data_start;
	ost << ", data_end = " << data_end;

	return ost.str();
}

std::string SampleBuffer::str_code(int code)
{
	switch (code) {
	case ERROR_TIMESTAMP:
		return "Sample buffer: Requested timestamp not valid";
	case ERROR_READ:
		return "Sample buffer: Read error";
	case ERROR_WRITE:
		return "Sample buffer: Write error";
	case ERROR_OVERFLOW:
		return "Sample buffer: Overrun";
	default:
		return "Sample buffer: Unknown error";
	}
}
