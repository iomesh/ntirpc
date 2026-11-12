/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef NTIRPC_SVC_OBSERVE_INTERNAL_H
#define NTIRPC_SVC_OBSERVE_INTERNAL_H

#include <rpc/svc_observe.h>

void svc_observe_begin(struct svc_observation *observation);
uint64_t svc_observe_stage_start(const struct svc_observation *observation);
uint64_t svc_observe_event_start(void);
void svc_observe_stage_end(enum svc_observe_stage stage, uint64_t started);
void svc_observe_event(enum svc_observe_stage event);
void svc_observe_reply_queued(struct svc_observation *observation);
void svc_observe_send_begin(struct svc_observation *observation);
void svc_observe_end(struct svc_observation *observation,
		     enum svc_observe_result result);
void svc_observe_receive_begin(struct svc_receive_observation *receive,
			       uint64_t started);
void svc_observe_receive_resume(struct svc_receive_observation *receive);
void svc_observe_receive_pause(struct svc_receive_observation *receive);
void svc_observe_receive_end(struct svc_receive_observation *receive,
			     bool complete);
#endif
