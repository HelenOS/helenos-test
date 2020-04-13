/*
 * Copyright (c) 2015 Michal Koutny
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * - The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <adt/list.h>
#include <assert.h>
#include <errno.h>
#include <stdlib.h>

#include "repo.h"
#include "edge.h"
#include "job_closure.h"
#include "log.h"

/** Struct describes how to traverse units graph */
struct bfs_ops;
typedef struct bfs_ops bfs_ops_t;

struct bfs_ops {
	enum {
		BFS_FORWARD,  /**< Follow oriented edges */
		BFS_BACKWARD  /**< Go against oriented edges */
	} direction;

	/** Visit a unit via edge
	 * unit, incoming edge, traversing ops, user data
	 * return result of visit (error stops further traversal)
	 */
	errno_t (*visit)(unit_t *, unit_edge_t *, bfs_ops_t *, void *);

	/** Clean units remaining in BFS queue after error */
	void (*clean)(unit_t *, bfs_ops_t *, void *);
};

/*
 * Static functions
 */

static errno_t job_add_blocked_job(job_t *blocking_job, job_t *blocked_job)
{
	assert(blocking_job->blocked_jobs.size ==
	    blocking_job->blocked_jobs_count);

	errno_t rc = array_append(&blocking_job->blocked_jobs, job_t *,
	    blocked_job);
	if (rc != EOK) {
		return ENOMEM;
	}
	job_add_ref(blocked_job);

	blocking_job->blocked_jobs_count += 1;
	blocked_job->blocking_jobs += 1;

	return EOK;
}

/** During visit creates job and appends it to closure
 *
 * @note Assumes BFS origin unit's job is already present in the closure on the
 *       last position!
 *
 * @return EOK on success
 */
static errno_t visit_propagate_job(unit_t *u, unit_edge_t *e, bfs_ops_t *ops,
    void *arg)
{
	errno_t rc = EOK;
	job_t *created_job = NULL;
	job_closure_t *closure = arg;

	if (e == NULL) {
		if (u->bfs_data != NULL) {
			goto finish;
		}
		job_t *first_job = array_last(closure, job_t *);

		job_add_ref(first_job);
		u->bfs_data = first_job;

		goto finish;
	}

	job_t *job = (ops->direction == BFS_FORWARD) ?
	    e->input->bfs_data :
	    e->output->bfs_data;

	assert(job != NULL);

	if (u->bfs_data == NULL) {
		created_job = job_create(u, job->target_state);
		if (created_job == NULL) {
			rc = ENOMEM;
			goto finish;
		}

		/* Pass job reference to closure and add one for unit */
		rc = array_append(closure, job_t *, created_job);
		if (rc != EOK) {
			goto finish;
		}

		job_add_ref(created_job);
		u->bfs_data = created_job;
	}

	/* Depending on edge type block existing jobs */
	rc = job_add_blocked_job(u->bfs_data, job);

finish:
	if (rc != EOK) {
		job_del_ref(&created_job);
	}
	return rc;
}

static errno_t visit_isolate(unit_t *u, unit_edge_t *e, bfs_ops_t *ops, void *arg)
{
	errno_t rc = EOK;
	job_t *created_job = NULL;
	job_closure_t *closure = arg;

	sysman_log(LVL_DEBUG2, "%s(%s)", __func__, unit_name(u));
	/*
	 * Unit can have starting job from original request or from isolation
	 * BFS with different origin.
	 *
	 * Don't check u->state == STATE_STOPPED, closure is created stateless
	 * and its upon merging procedure to correctly resolve conflicting
	 * jobs.
	 *
	 * If we're at the origin (no BFS incoming edge), create a stop job,
	 * put it to the closure and let propagate as if called the propagate
	 * visitor.
	 */
	if (e == NULL && u->bfs_data == NULL) {
		created_job = job_create(u, STATE_STOPPED);
		if (created_job == NULL) {
			rc = ENOMEM;
			goto finish;
		}

		/* Pass job reference to closure and add one for unit */
		rc = array_append(closure, job_t *, created_job);
		if (rc != EOK) {
			goto finish;
		}
	}
	rc = visit_propagate_job(u, e, ops, closure);

finish:
	if (rc != EOK) {
		job_del_ref(&created_job);
	}
	sysman_log(LVL_DEBUG2, "%s(%s) -> %i", __func__, unit_name(u), rc);
	return rc;
}

static void traverse_clean(unit_t *u, bfs_ops_t *ops, void *arg)
{
	job_t *job = u->bfs_data;
	job_del_ref(&job);
	u->bfs_data = NULL;
}

static errno_t bfs_traverse_component_internal(unit_t *origin, bfs_ops_t *ops,
    void *arg)
{
	errno_t rc;
	list_t units_fifo;
	list_initialize(&units_fifo);

	unit_t *unit = origin;

	rc = ops->visit(unit, NULL, ops, arg);
	if (rc != EOK) {
		goto finish;
	}
	unit->bfs_tag = true;
	list_append(&unit->bfs_link, &units_fifo);

	while (!list_empty(&units_fifo)) {
		unit = list_get_instance(list_first(&units_fifo), unit_t,
		    bfs_link);
		list_remove(&unit->bfs_link);

		if (ops->direction == BFS_FORWARD) {
			list_foreach(unit->edges_out, edges_out, unit_edge_t, e) {
				unit_t *u = e->output;
				if (!u->bfs_tag) {
					u->bfs_tag = true;
					list_append(&u->bfs_link, &units_fifo);
				}
				rc = ops->visit(u, e, ops, arg);
				if (rc != EOK) {
					goto finish;
				}
			}
		} else {
			list_foreach(unit->edges_in, edges_in, unit_edge_t, e) {
				unit_t *u = e->input;
				if (!u->bfs_tag) {
					u->bfs_tag = true;
					list_append(&u->bfs_link, &units_fifo);
				}
				rc = ops->visit(u, e, ops, arg);
				if (rc != EOK) {
					goto finish;
				}
			}
		}
	}
	rc = EOK;

finish:
	/* Let user clean partially processed units */
	list_foreach_safe(units_fifo, cur_link, next_link) {
		unit_t *u = list_get_instance(cur_link, unit_t, bfs_link);
		ops->clean(u, ops, arg);
		list_remove(cur_link);
	}

	return rc;
}

static errno_t bfs_traverse_component(unit_t *origin, bfs_ops_t *ops, void *arg)
{
	/* Check invariant */
	repo_foreach(u) {
		assert(u->bfs_tag == false);
	}
	errno_t rc = bfs_traverse_component_internal(origin, ops, arg);

	/* Clean after ourselves (BFS tag jobs) */
	repo_foreach(u) {
		u->bfs_tag = false;
	}
	return rc;
}

static errno_t bfs_traverse_all(bfs_ops_t *ops, void *arg)
{
	/* Check invariant */
	repo_foreach(u) {
		assert(u->bfs_tag == false);
	}
	errno_t rc = EOK;

	repo_foreach(origin) {
		sysman_log(LVL_DEBUG2, "%s: %p, %i", __func__, origin, origin->bfs_tag);
		if (origin->bfs_tag == true) {
			continue;
		}
		rc = bfs_traverse_component_internal(origin, ops, arg);
		if (rc != EOK) {
			goto finish;
		}
	}

finish:
	/* Clean after ourselves (BFS tag jobs) */
	repo_foreach(u) {
		u->bfs_tag = false;
	}
	return rc;
}

/*
 * Non-static functions
 */

/** Creates job closure for given basic job
 *
 * @note It is caller's responsibility to clean job_closure (even on error).
 *
 * @return EOK on success otherwise propagated error
 */
errno_t job_create_closure(job_t *main_job, job_closure_t *job_closure, int flags)
{
	sysman_log(LVL_DEBUG2, "%s(%s)", __func__, unit_name(main_job->unit));

	if ((flags & CLOSURE_ISOLATE) && main_job->target_state != STATE_STARTED) {
		// TODO EINVAL?
		return ENOTSUP;
	}

	errno_t rc = array_append(job_closure, job_t *, main_job);
	if (rc != EOK) {
		return rc;
	}
	job_add_ref(main_job); /* Add one for the closure */

	/* Propagate main_job to other (dependent) units */
	static bfs_ops_t propagate_ops = {
		.clean = traverse_clean,
		.visit = visit_propagate_job
	};
	switch (main_job->target_state) {
	case STATE_STARTED:
		propagate_ops.direction = BFS_FORWARD;
		break;
	case STATE_STOPPED:
		propagate_ops.direction = BFS_BACKWARD;
		break;
	default:
		assert(false);
	}

	rc = bfs_traverse_component(main_job->unit, &propagate_ops, job_closure);

	sysman_log(LVL_DEBUG2, "%s: %i&%i", __func__, flags, CLOSURE_ISOLATE);
	if (flags & CLOSURE_ISOLATE) {
		static bfs_ops_t isolate_ops = {
			.direction = BFS_BACKWARD,
			.clean = traverse_clean,
			.visit = visit_isolate
		};
		rc = bfs_traverse_all(&isolate_ops, job_closure);
	}

	if (rc == EOK) {
		array_foreach(*job_closure, job_t *, job_it) {
			sysman_log(LVL_DEBUG2, "%s\t%s, refs: %u", __func__,
			    unit_name((*job_it)->unit), atomic_load(&(*job_it)->refcnt));
		}
	}

	/* Clean after ourselves (BFS tag jobs) */
	array_foreach(*job_closure, job_t *, job_it) {
		job_t *j = (*job_it)->unit->bfs_data;
		assert(*job_it == j);
		job_del_ref(&j);
		(*job_it)->unit->bfs_data = NULL;
	}

	return rc;
}