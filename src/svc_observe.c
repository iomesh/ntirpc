// SPDX-License-Identifier: BSD-3-Clause
#include "svc_observe_internal.h"
#include <assert.h>
#include <time.h>
#include <stddef.h>
#include <errno.h>

static svc_observe_callback observer;
static bool observe_overall;
static bool observe_stages;
static bool observe_cpu;
static uint32_t observe_sample_every = 64;
static __thread uint64_t random_state;

static void observe_emit(enum svc_observe_stage stage,
			 enum svc_observe_result result, uint64_t duration)
{
	int saved_errno = errno;

	observer(stage, result, duration);
	errno = saved_errno;
}

static uint64_t observe_clock(void)
{
	struct timespec timestamp;

	clock_gettime(CLOCK_MONOTONIC, &timestamp);
	return (uint64_t)timestamp.tv_sec * 1000000000 + timestamp.tv_nsec;
}

static bool select_sample(void)
{
	if (!observer) {
		return false;
	}
	if (observe_sample_every == 1) {
		return true;
	}
	if (!random_state) {
		random_state = (uintptr_t)&random_state;
		if (!random_state) {
			random_state = 1;
		}
	}
	random_state ^= random_state << 13;
	random_state ^= random_state >> 7;
	random_state ^= random_state << 17;
	return random_state % observe_sample_every == 0;
}

void svc_observe_register(svc_observe_callback callback, bool overall,
			  bool stages, bool cpu, uint32_t sample_every)
{
	assert(sample_every > 0);
	observer = callback;
	observe_overall = overall;
	observe_stages = stages;
	observe_cpu = cpu;
	observe_sample_every = sample_every;
}

void svc_observe_begin(struct svc_observation *observation)
{
	*observation = (struct svc_observation){0};
	if (observer) {
		observation->active = true;
		observation->sampled = select_sample();
		if (observation->sampled && (observe_overall || observe_stages)) {
			observation->started = observe_clock();
		}
	}
}

uint64_t svc_observe_stage_start(const struct svc_observation *observation)
{
	return observer && observe_stages && observation->sampled ?
		observe_clock() : 0;
}

uint64_t svc_observe_event_start(void)
{
	return observe_stages && select_sample() ? observe_clock() : 0;
}

static uint64_t observe_cpu_clock(void)
{
	struct timespec timestamp;
	int saved_errno = errno;
	uint64_t value = 0;

	if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &timestamp) == 0) {
		value = (uint64_t)timestamp.tv_sec * 1000000000 + timestamp.tv_nsec;
	} else {
		observe_emit(SVC_OBSERVE_CPU_UNAVAILABLE, SVC_OBSERVE_ERROR, 0);
	}
	errno = saved_errno;
	return value;
}

uint64_t svc_observe_cpu_start(const struct svc_observation *observation)
{
	return observer && observe_cpu && observation->sampled ?
		observe_cpu_clock() : 0;
}

void svc_observe_cpu_end(enum svc_observe_stage stage, uint64_t started)
{
	if (observer && started) {
		uint64_t completed = observe_cpu_clock();

		if (completed) {
			observe_emit(stage, SVC_OBSERVE_SUBMITTED, completed - started);
		}
	}
}

void svc_observe_stage_end(enum svc_observe_stage stage, uint64_t started)
{
	if (observer && started) {
		observe_emit(stage, SVC_OBSERVE_SUBMITTED, observe_clock() - started);
	}
}

void svc_observe_event(enum svc_observe_stage event)
{
	if (observer) {
		observe_emit(event, SVC_OBSERVE_SUBMITTED, 0);
	}
}

void svc_observe_application_begin(struct svc_observation *observation,
				   uint64_t timestamp)
{
	if (observer && observe_stages && observation->sampled) {
		observation->application_started = timestamp;
	}
}

void svc_observe_application_end(struct svc_observation *observation,
				 uint64_t timestamp)
{
	if (observation->application_started &&
	    !observation->application_completed) {
		observation->application_completed = timestamp;
	}
}

void svc_observe_reply_queued(struct svc_observation *observation)
{
	if (observer && observe_stages && observation->sampled) {
		observation->queued = observe_clock();
	}
}

void svc_observe_send_begin(struct svc_observation *observation)
{
	if (observation->queued && !observation->writing) {
		observation->writing = observe_clock();
	}
}

void svc_observe_end(struct svc_observation *observation,
		     enum svc_observe_result result)
{
	if (observer && observation->active) {
		const struct svc_observation finished = *observation;
		uint64_t completed = finished.started ? observe_clock() : 0;

		*observation = (struct svc_observation){0};
		if (finished.started && observe_overall) {
			observe_emit(SVC_OBSERVE_TOTAL, result,
				     completed - finished.started);
		}
		/* Only a fully bounded submitted request belongs to the normal
		 * additive stage cohort. Early errors/drops have overall/result
		 * coverage, never fabricated zero protocol/response intervals. */
		if (observe_stages && result == SVC_OBSERVE_SUBMITTED &&
		    finished.application_completed && finished.queued &&
		    finished.writing) {
			observe_emit(SVC_OBSERVE_DISPATCH_SETUP, result,
				     finished.application_started - finished.started);
			observe_emit(SVC_OBSERVE_REPLY_ENCODE, result,
				     finished.queued - finished.application_completed);
			observe_emit(SVC_OBSERVE_SEND_QUEUE, result,
				     finished.writing - finished.queued);
			observe_emit(SVC_OBSERVE_SEND_FLUSH, result,
				     completed - finished.writing);
		} else if (observe_stages && finished.sampled) {
			observe_emit(SVC_OBSERVE_STAGE_INCOMPLETE, result, 0);
		}
		observe_emit(SVC_OBSERVE_COMPLETION, result, 0);
	}
}

void svc_observe_receive_begin(struct svc_receive_observation *receive,
			       uint64_t started)
{
	*receive = (struct svc_receive_observation){
		.started = started,
		.segment = started,
	};
}

void svc_observe_receive_resume(struct svc_receive_observation *receive)
{
	if (receive->started) {
		receive->segment = observe_clock();
	}
}

void svc_observe_receive_pause(struct svc_receive_observation *receive)
{
	if (receive->segment) {
		receive->active += observe_clock() - receive->segment;
		receive->segment = 0;
	}
}

void svc_observe_receive_end(struct svc_receive_observation *receive,
			     bool complete)
{
	if (!receive->started) {
		return;
	}
	svc_observe_receive_pause(receive);
	uint64_t elapsed = observe_clock() - receive->started;
	uint64_t active = receive->active;

	receive->started = 0;
	if (!complete) {
		svc_observe_event(SVC_OBSERVE_RECV_INCOMPLETE);
		return;
	}
	observe_emit(SVC_OBSERVE_RECV_RECORD, SVC_OBSERVE_SUBMITTED, elapsed);
	observe_emit(SVC_OBSERVE_RECV_SEGMENT_WALL, SVC_OBSERVE_SUBMITTED, active);
	observe_emit(SVC_OBSERVE_RECV_OUTSIDE_SEGMENTS, SVC_OBSERVE_SUBMITTED,
		     elapsed - active);
}
