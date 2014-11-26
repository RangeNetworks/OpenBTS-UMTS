#ifndef UHD_BUFFER_H
#define UHD_BUFFER_H

#include <string>
#include <complex>
#include <uhd/types/time_spec.hpp>

/*
 * Sample Buffer
 *
 * Allows reading and writing of timed samples using sample ticks or UHD
 * timespec values.
 */

class SampleBuffer {
public:
	SampleBuffer(int len, double rate);
	~SampleBuffer();

	/* Return number of samples available for a given ts */
	int avail_smpls(long long ts) const;
	int avail_smpls(uhd::time_spec_t ts) const;

	int read(void *buf, int len, long long ts);
	int read(void *buf, int len, uhd::time_spec_t ts);
	int write(void *buf, int len, long long ts);
	int write(void *buf, int len, uhd::time_spec_t ts);

	/* Return formatted string describing internal buffer state */
	std::string str_status(long long ts) const;

	/* Formatted error code string */
	static std::string str_code(int code);

	enum err_code {
		ERROR_TIMESTAMP = -1,
		ERROR_READ = -2,
		ERROR_WRITE = -3,
		ERROR_OVERFLOW = -4
	};

private:
	std::complex<short> *data;
	int len;
	double clock_rate;

	long long time_start;
	long long time_end;
	long long data_start;
	long long data_end;
};

#endif /* UHD_BUFFER_H */
