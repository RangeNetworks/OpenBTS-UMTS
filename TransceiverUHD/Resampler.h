#ifndef _RESAMPLER_H_
#define _RESAMPLER_H_

class Resampler {
public:
	/* Constructor for rational sample rate conversion
	 *   @param p numerator of resampling ratio
	 *   @param q denominator of resampling ratio
	 *   @param filt_len length of each polyphase subfilter 
	 */
	Resampler(size_t p, size_t q, size_t filt_len = 16);
	~Resampler();

	/* Initilize resampler filterbank.
	 *   @param bw bandwidth factor on filter generation (pre-window)
	 *   @return false on error, zero otherwise
	 *
	 * Automatic setting is to compute the filter to prevent aliasing with
	 * a Blackman-Harris window. Adjustment is made through a bandwith
	 * factor to shift the cutoff and/or the constituent filter lengths.
	 * Calculation of specific rolloff factors or 3-dB cutoff points is
	 * left as an excersize for the reader.
	 */
	bool init(int type = FILTER_TYPE_SINC, float bw = 1.0f);

	/* Rotate "commutator" and drive samples through filterbank
	 *   @param in continuous buffer of input complex float values
	 *   @param in_len input buffer length
	 *   @param out continuous buffer of output complex float values
	 *   @param out_len output buffer length
	 *   @return number of samples outputted, negative on error
         *
	 * Input and output vector lengths must of be equal multiples of the
	 * rational conversion rate denominator and numerator respectively.
	 */
	int rotate(float *in, size_t in_len, float *out, size_t out_len);

	/* Get filter length
	 *   @return number of taps in each filter partition 
	 */
	size_t len();

	enum {
		FILTER_TYPE_SINC,
		FILTER_TYPE_RRC,
	};

private:
	size_t p;
	size_t q;
	size_t filt_len;
	size_t path_len;
	size_t *in_index;
	size_t *out_path;

	float **partitions;
	float *history;

	bool initFilters(int type, float bw);
	void releaseFilters();
	bool computePaths(int len);
	bool checkLen(size_t in_len, size_t out_len);
};

#endif /* _RESAMPLER_H_ */
