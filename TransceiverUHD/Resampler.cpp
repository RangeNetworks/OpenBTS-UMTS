/*
 * Rational Sample Rate Conversion
 * Copyright (C) 2012, 2013 Thomas Tsou <tom@tsou.cc>
 * Copyright (C) 2014 Ettus Research LLC
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <malloc.h>
#include <iostream>

#include "Resampler.h"

extern "C" {
#include "convolve.h"
}

#ifndef M_PI
#define M_PI			3.14159265358979323846264338327f
#endif

#define MAX_OUTPUT_LEN		8192

static float sinc(float x)
{
	if (x == 0.0)
		return 0.9999999999;

	return sin(M_PI * x) / (M_PI * x);
}

/* 
 * Generate sinc prototype filter with a Blackman-harris window.
 * Scale coefficients with DC filter gain set to unity divided
 * by the number of filter partitions. 
 */
static float gen_windowed_sinc(float *buf, size_t len, int p, int q, float bw)
{
	float sum = 0.0f;
	float a0 = 0.35875f;
	float a1 = 0.48829f;
	float a2 = 0.14128f;
	float a3 = 0.01168f;
	float cutoff;
	float midpt = ((float) len - 1.0f) / 2.0f;

	if (p > q)
		cutoff = (float) p;
	else
		cutoff = (float) q;

	for (size_t i = 0; i < len; i++) {
		buf[i] = sinc(((float) i - midpt) / cutoff * bw);
		buf[i] *= a0 -
			  a1 * cos(2.0f * M_PI * i / (float) (len - 1)) +
			  a2 * cos(4.0f * M_PI * i / (float) (len - 1)) -
			  a3 * cos(6.0f * M_PI * i / (float) (len - 1));
		sum += buf[i];
	}

	return (float) p / sum;
}

/* 
 * Generate UMTS root raised cosine prototype filter.
 */
static float gen_umts_rrc_pulse(float *buf, size_t len, int p, int q, float bw)
{
	float alpha = 0.22f;
	float Tc = 1.0f;
	float sum = 0.0f;
	float midpt = ((float) len - 1.0f) / 2.0f;

	for (size_t i = 0; i < len; i++) {
		float t, a, b, c;

		t = ((float) i - midpt) / (float) p * bw;
		a = sinf(M_PI * t / Tc * (1.0f - alpha));
		b = 4.0f * alpha * t / Tc * cosf(M_PI * t / Tc * (1 + alpha));
		c = 4.0f * alpha * t / Tc;

		buf[i] = (a + b) / (M_PI * t / Tc * (1.0f - c * c));
		sum += buf[i];
	}

	return (float) p / sum;
}

bool Resampler::initFilters(int type, float bw)
{
	size_t proto_len = p * filt_len;
	float *proto, scale;

	/* 
	 * Allocate partition filters and the temporary prototype filter
	 * according to numerator of the rational rate. Coefficients are
	 * real only and must be 16-byte memory aligned for SSE usage.
	 */
	proto = new float[proto_len];
	if (!proto)
		return false;

	partitions = (float **) malloc(sizeof(float *) * p);
	if (!partitions) {
		delete proto;
		return false;
	}

	for (size_t i = 0; i < p; i++) {
		partitions[i] = (float *)
				memalign(16, filt_len * 2 * sizeof(float));
	}

	/* Filter type selection */
	if (type == FILTER_TYPE_RRC) {
		scale = gen_umts_rrc_pulse(proto, proto_len, p, q, bw);
	} else if (type == FILTER_TYPE_SINC) {
		scale = gen_windowed_sinc(proto, proto_len, p, q, bw);
	} else {
		delete proto;
		return false;
	}

	/* Populate filter partitions from the prototype filter */
	for (size_t i = 0; i < filt_len; i++) {
		for (size_t n = 0; n < p; n++) {
			partitions[n][2 * i + 0] = proto[i * p + n] * scale;
			partitions[n][2 * i + 1] = 0.0f;
		}
	}

	/* Reverse filter taps for convolution */
	for (size_t n = 0; n < p; n++) {
		for (size_t i = 0; i < filt_len / 2; i++) {
			float a = partitions[n][2 * i];
			float b = partitions[n][2 * (filt_len - 1 - i)];

			partitions[n][2 * i] = b;
			partitions[n][2 * (filt_len - 1 - i)] = a;
		}
	}

	delete proto;
	return true;
}

void Resampler::releaseFilters()
{
	if (partitions) {
		for (size_t i = 0; i < p; i++)
			free(partitions[i]);
	}

	free(partitions);
	partitions = NULL;
}

bool Resampler::checkLen(size_t in_len, size_t out_len)
{
	if (in_len % q) {
#ifdef DEBUG
		std::cerr << "Invalid input length " << in_len
			  <<  " is not multiple of " << q << std::endl;
#endif
		return false;
	}

	if (out_len % p) {
#ifdef DEBUG
		std::cerr << "Invalid output length " << out_len
			  <<  " is not multiple of " << p << std::endl;
#endif
		return false;
	}

	if ((in_len / q) != (out_len / p)) {
#ifdef DEBUG
		std::cerr << "Input/output block length mismatch" << std::endl;
		std::cerr << "P = " << p << ", Q = " << q << std::endl;
		std::cerr << "Input len: " << in_len << std::endl;
		std::cerr << "Output len: " << out_len << std::endl;
#endif
		return false;
	}

	if (out_len > path_len) {
#ifdef DEBUG
		std::cerr << "Block length of " << out_len
			  << " exceeds max of " << path_len << std::endl;
		std::cerr << "Resizing" << std::endl;
#endif
		computePaths(out_len * 2);
	}

	return true;
}

int Resampler::rotate(float *in, size_t in_len, float *out, size_t out_len)
{
	int n, path;
	int hist_len = filt_len - 1;

	if (!checkLen(in_len, out_len))
		return -1;

	/* Insert history */
	memcpy(&in[-2 * hist_len], history, hist_len * 2 * sizeof(float));

	/* Generate output from precomputed input/output paths */
	for (size_t i = 0; i < out_len; i++) {
		n = in_index[i];
		path = out_path[i];

		convolve_real(in, in_len,
			      partitions[path], filt_len,
			      &out[2 * i], out_len - i,
			      n, 1, 1, 0);
	}

	/* Save history */
	memcpy(history, &in[2 * (in_len - hist_len)],
	       hist_len * 2 * sizeof(float));

	return out_len;
}

bool Resampler::init(int type, float bw)
{
	size_t hist_len = filt_len - 1;

	/* Filterbank filter internals */
	if (!initFilters(type, bw))
		return false;

	/* History buffer */
	history = new float[2 * hist_len];
	memset(history, 0, 2 * hist_len * sizeof(float));

	computePaths(path_len);

	return true;
}

bool Resampler::computePaths(int len)
{
	if (len <= 0)
		return false;

	delete in_index;
	delete out_path;

	/* Precompute filterbank paths */
	in_index = new size_t[len];
	out_path = new size_t[len];

	for (int i = 0; i < len; i++) {
		in_index[i] = (q * i) / p;
		out_path[i] = (q * i) % p;
	}

	path_len = len;

	return true;
}

size_t Resampler::len()
{
	return filt_len;
}

Resampler::Resampler(size_t p, size_t q, size_t filt_len)
	: in_index(NULL), out_path(NULL), partitions(NULL), history(NULL)
{
	this->p = p;
	this->q = q;
	this->filt_len = filt_len;
	this->path_len = MAX_OUTPUT_LEN;
}

Resampler::~Resampler()
{
	releaseFilters();

	delete history;
	delete in_index;
	delete out_path;
}
