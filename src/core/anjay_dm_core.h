/*
 * Copyright 2017-2024 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay LwM2M SDK
 * All rights reserved.
 *
 * Licensed under the AVSystem-5-clause License.
 * See the attached LICENSE file for details.
 */

#ifndef ANJAY_DM_H
#define ANJAY_DM_H

#include <limits.h>

#include <avsystem/commons/avs_list.h>
#include <avsystem/commons/avs_stream.h>

#include <anjay_modules/anjay_dm_utils.h>
#include <anjay_modules/anjay_servers.h>

#include <avsystem/coap/streaming.h>

#include "coap/anjay_msg_details.h"
#include "dm/anjay_dm_attributes.h"

VISIBILITY_PRIVATE_HEADER_BEGIN

typedef struct {
    anjay_dm_module_deleter_t *deleter;
    void *arg;
} anjay_dm_installed_module_t;

struct anjay_dm {
    AVS_LIST(anjay_dm_installed_object_t) objects;
    AVS_LIST(anjay_dm_installed_module_t) modules;
};

void _anjay_dm_cleanup(anjay_dm_t *dm);

typedef struct {
    bool has_min_period;
    bool has_max_period;
    bool has_greater_than;
    bool has_less_than;
    bool has_step;
    bool has_min_eval_period;
    bool has_max_eval_period;
#ifdef ANJAY_WITH_CON_ATTR
    bool has_con;
#endif // ANJAY_WITH_CON_ATTR
    anjay_dm_r_attributes_t values;
} anjay_request_attributes_t;

typedef struct {
    avs_coap_streaming_request_ctx_t *ctx;
    avs_stream_t *payload_stream;

    uint8_t request_code;

    bool is_bs_uri;

    anjay_uri_path_t uri;

    anjay_request_action_t action;
    uint16_t content_format;
    uint16_t requested_format;
    const avs_coap_observe_id_t *observe;

    anjay_request_attributes_t attributes;
} anjay_request_t;

#define REQUEST_TO_ACTION_INFO(Request, Ssid)    \
    (const anjay_action_info_t) {                \
        .oid = (Request)->uri.ids[ANJAY_ID_OID], \
        .iid = (Request)->uri.ids[ANJAY_ID_IID], \
        .ssid = (Ssid),                          \
        .action = (Request)->action              \
    }

int _anjay_dm_transaction_validate(anjay_unlocked_t *anjay);
int _anjay_dm_transaction_finish_without_validation(anjay_unlocked_t *anjay,
                                                    int result);

static inline int _anjay_dm_transaction_rollback(anjay_unlocked_t *anjay) {
    int result = _anjay_dm_transaction_finish(anjay, INT_MIN);
    return (result == INT_MIN) ? 0 : result;
}

int _anjay_dm_perform_action(anjay_connection_ref_t connection,
                             const anjay_request_t *request);

static inline int _anjay_dm_map_present_result(int result) {
    if (!result) {
        return ANJAY_ERR_NOT_FOUND;
    } else if (result > 0) {
        return 0;
    } else {
        return result;
    }
}

AVS_LIST(anjay_dm_installed_module_t) *
_anjay_dm_module_find_ptr(anjay_unlocked_t *anjay,
                          anjay_dm_module_deleter_t *module_deleter);

int _anjay_dm_select_free_iid(anjay_unlocked_t *anjay,
                              const anjay_dm_installed_object_t *obj,
                              anjay_iid_t *new_iid_ptr);

typedef struct {
    anjay_uri_path_t uri;

    // True if the entire path queried by @ref _anjay_dm_path_info() is present.
    bool is_present;

    // True if a leaf of the queried path is not a simple value.
    bool is_hierarchical;
    // True if the path points to a present resource or multiple resource.
    bool has_resource;
    // Only valid if has_resource == true.
    anjay_dm_resource_kind_t kind;
} anjay_dm_path_info_t;

int _anjay_dm_path_info(anjay_unlocked_t *anjay,
                        const anjay_dm_installed_object_t *obj,
                        const anjay_uri_path_t *path,
                        anjay_dm_path_info_t *out_info);

uint8_t _anjay_dm_make_success_response_code(anjay_request_action_t action);

#define dm_log(...) _anjay_log(anjay_dm, __VA_ARGS__)

VISIBILITY_PRIVATE_HEADER_END

#endif // ANJAY_DM_H
