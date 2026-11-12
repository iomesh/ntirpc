/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef NTIRPC_SVC_OBSERVE_H
#define NTIRPC_SVC_OBSERVE_H

#include <stdbool.h>
#include <stdint.h>

/* Callbacks are registered once before starting service threads. The provider
 * owns aggregation; libntirpc has no dependency on an application exporter.
 */
enum svc_observe_stage {
	SVC_OBSERVE_TOTAL,
	SVC_OBSERVE_RPC_DECODE,
	SVC_OBSERVE_REPLY_ENCODE,
	SVC_OBSERVE_SEND_QUEUE,
	SVC_OBSERVE_SEND_FLUSH,
	SVC_OBSERVE_RECV_SCHEDULE,
	SVC_OBSERVE_RESUME_SCHEDULE,
	SVC_OBSERVE_RECV_DIRECT,
	SVC_OBSERVE_SEND_BLOCKED,
	SVC_OBSERVE_RECV_RECORD,
	SVC_OBSERVE_RECV_SEGMENT_WALL,
	SVC_OBSERVE_RECV_OUTSIDE_SEGMENTS,
	SVC_OBSERVE_REQUEST_ALLOC,
	SVC_OBSERVE_RECV_INCOMPLETE,
	SVC_OBSERVE_COMPLETION,
	SVC_OBSERVE_DISPATCH_CPU,
	SVC_OBSERVE_RESUME_CPU,
	SVC_OBSERVE_CPU_UNAVAILABLE,
	SVC_OBSERVE_DISPATCH_SETUP,
	SVC_OBSERVE_STAGE_INCOMPLETE,
	SVC_OBSERVE_STAGE_COUNT
};

enum svc_observe_result {
	SVC_OBSERVE_SUBMITTED,
	SVC_OBSERVE_ERROR,
	SVC_OBSERVE_NO_REPLY
};

/* One sample decision follows a TCP record across partial receive events.
 * Segment wall includes blocking recv and preemption; it is not CPU time. */
struct svc_receive_observation {
	uint64_t started;
	uint64_t segment;
	uint64_t active;
};

struct svc_observation {
	uint64_t started;
	uint64_t application_started;
	uint64_t application_completed;
	uint64_t queued;
	uint64_t writing;
	bool sampled;
	bool active;
};

typedef void (*svc_observe_callback)(enum svc_observe_stage,
				    enum svc_observe_result, uint64_t);

#ifdef __cplusplus
extern "C" {
#endif
/* One decision is shared by the request, its output and the application.
 * Overall/stages/CPU independently gate their clocks. Completion events are
 * full-count; duration callbacks contain actual samples, not scaled counts. */
void svc_observe_register(svc_observe_callback callback, bool overall,
			  bool stages, bool cpu, uint32_t sample_every);
/* The application supplies its already acquired monotonic boundary stamps.
 * No application-specific protocol types or additional clock reads here. */
void svc_observe_application_begin(struct svc_observation *observation,
				   uint64_t timestamp);
void svc_observe_application_end(struct svc_observation *observation,
				 uint64_t timestamp);
#ifdef __cplusplus
}
#endif
#endif
