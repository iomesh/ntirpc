// SPDX-License-Identifier: BSD-3-Clause
#include "../src/svc_observe_internal.h"
#include <assert.h>
#include <stddef.h>
#include <errno.h>
#include <time.h>
#include <rpc/rpc.h>
#include <rpc/xdr_ioq.h>

_Static_assert(offsetof(struct xdr_ioq, xdrs) == 0, "XDR must remain first");
_Static_assert(offsetof(struct svc_req, rq_xprt) == 0, "Transport must remain first");

static unsigned int totals, completions, stages, blocked;
static volatile unsigned int wall_clocks;
static unsigned int records, incomplete;
static uint64_t record_elapsed, record_active, record_outside;
static uint64_t durations[SVC_OBSERVE_STAGE_COUNT];

int __real_clock_gettime(clockid_t id, struct timespec *timestamp);
int __wrap_clock_gettime(clockid_t id, struct timespec *timestamp)
{
	if (id == CLOCK_MONOTONIC)
		wall_clocks++;
	return __real_clock_gettime(id, timestamp);
}

static void collect(enum svc_observe_stage stage,
		    enum svc_observe_result result, uint64_t elapsed)
{
	(void)result;
	durations[stage] = elapsed;
	errno = ENOENT;
	if (stage == SVC_OBSERVE_TOTAL)
		totals++;
	else if (stage == SVC_OBSERVE_COMPLETION)
		completions++;
	else if (stage == SVC_OBSERVE_SEND_BLOCKED)
		blocked++;
	else if (stage == SVC_OBSERVE_RECV_RECORD) {
		records++;
		record_elapsed = elapsed;
	} else if (stage == SVC_OBSERVE_RECV_SEGMENT_WALL)
		record_active = elapsed;
	else if (stage == SVC_OBSERVE_RECV_OUTSIDE_SEGMENTS)
		record_outside = elapsed;
	else if (stage == SVC_OBSERVE_STAGE_INCOMPLETE)
		incomplete++;
	else
		stages++;
}

int main(void)
{
	struct svc_observation request, output;
	struct svc_receive_observation receive;

	svc_observe_register(NULL, true, true, 64);
	svc_observe_begin(&request);
	assert(!request.active && !request.sampled && !request.started);
	assert(!svc_observe_stage_start(&request));
	svc_observe_end(&request, SVC_OBSERVE_NO_REPLY);
	assert(wall_clocks == 0 && completions == 0);

	for (unsigned int flags = 0; flags < 4; flags++) {
		bool overall = flags & 1, detail = flags & 2;
		unsigned int before_wall = wall_clocks;
		unsigned int before_totals = totals, before_stages = stages;
		unsigned int before_completions = completions;

		svc_observe_register(collect, overall, detail, 1);
		svc_observe_begin(&request);
		assert(request.sampled && request.active);
		assert(!!request.started == (overall || detail));
		svc_observe_stage_end(SVC_OBSERVE_RPC_DECODE,
				      svc_observe_stage_start(&request));

		output = request;
		request = (struct svc_observation){0};
		svc_observe_end(&request, SVC_OBSERVE_NO_REPLY);
		errno = EINTR;
		svc_observe_end(&output, SVC_OBSERVE_SUBMITTED);
		assert(errno == EINTR);
		svc_observe_end(&output, SVC_OBSERVE_NO_REPLY);
		assert(totals - before_totals == (unsigned int)overall);
		assert(stages - before_stages == (unsigned int)detail);

		assert(completions - before_completions == 1);
		assert(wall_clocks - before_wall == 2 * (unsigned int)(overall || detail) + 2 * (unsigned int)detail);

	}

	svc_observe_register(collect, true, true, 64);
	unsigned int selected = 0, n = 65536;
	unsigned int before_wall = wall_clocks;
	unsigned int before_totals = totals, before_completions = completions;

	for (unsigned int index = 0; index < n; index++) {
		svc_observe_begin(&request);
		selected += request.sampled;
		if (!request.sampled)
			assert(request.started == 0);
		svc_observe_stage_end(SVC_OBSERVE_RPC_DECODE,
				      svc_observe_stage_start(&request));
		svc_observe_end(&request, SVC_OBSERVE_SUBMITTED);
	}
	assert(selected > 256 && selected < 4096);
	assert(totals - before_totals == selected);
	assert(completions - before_completions == n);
	assert(wall_clocks - before_wall == 4 * selected);


	/* Real boundary stamps, transferred to output, close exactly. Neither
	 * application marker takes a second clock or emits another duration. */
	svc_observe_register(collect, true, true, 1);
	svc_observe_begin(&request);
	struct timespec timestamp;

	clock_gettime(CLOCK_MONOTONIC, &timestamp);
	uint64_t application_started = (uint64_t)timestamp.tv_sec * 1000000000 + timestamp.tv_nsec;
	unsigned int marker_clocks = wall_clocks;

	svc_observe_application_begin(&request, application_started);
	assert(wall_clocks == marker_clocks);
	clock_gettime(CLOCK_MONOTONIC, &timestamp);
	uint64_t application_completed = (uint64_t)timestamp.tv_sec * 1000000000 + timestamp.tv_nsec;

	marker_clocks = wall_clocks;
	svc_observe_application_end(&request, application_completed);
	svc_observe_application_end(&request, application_completed);
	assert(wall_clocks == marker_clocks);
	output = request;
	request = (struct svc_observation){0};
	svc_observe_reply_queued(&output);
	svc_observe_send_begin(&output);
	marker_clocks = wall_clocks;
	svc_observe_send_begin(&output);
	assert(wall_clocks == marker_clocks);
	svc_observe_end(&output, SVC_OBSERVE_SUBMITTED);
	assert(durations[SVC_OBSERVE_TOTAL] ==
	       durations[SVC_OBSERVE_DISPATCH_SETUP] +
	       application_completed - application_started +
	       durations[SVC_OBSERVE_REPLY_ENCODE] +
	       durations[SVC_OBSERVE_SEND_QUEUE] +
	       durations[SVC_OBSERVE_SEND_FLUSH]);

	svc_observe_register(collect, false, false, 1);
	before_wall = wall_clocks;
	assert(!svc_observe_event_start());
	assert(wall_clocks == before_wall);
	svc_observe_event(SVC_OBSERVE_SEND_BLOCKED);
	assert(blocked == 1);
	svc_observe_register(collect, false, true, 1);
	svc_observe_receive_begin(&receive, svc_observe_event_start());
	svc_observe_receive_pause(&receive);
	svc_observe_receive_resume(&receive);
	svc_observe_receive_end(&receive, true);
	svc_observe_receive_end(&receive, true);
	assert(records == 1 && record_elapsed == record_active + record_outside);
	return 0;
}
