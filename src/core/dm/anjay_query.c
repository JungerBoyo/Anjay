/*
 * Copyright 2017-2024 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay LwM2M SDK
 * All rights reserved.
 *
 * Licensed under the AVSystem-5-clause License.
 * See the attached LICENSE file for details.
 */

#include <anjay_init.h>

#include <anjay_modules/anjay_time_defs.h>

#include "anjay_query.h"

#include "../anjay_core.h"
#include "../anjay_dm_core.h"

VISIBILITY_SOURCE_BEGIN

typedef struct {
    anjay_ssid_t ssid;
    anjay_iid_t out_iid;
} find_iid_args_t;

static int find_server_iid_handler(anjay_unlocked_t *anjay,
                                   const anjay_dm_installed_object_t *obj,
                                   anjay_iid_t iid,
                                   void *args_) {
    (void) obj;
    find_iid_args_t *args = (find_iid_args_t *) args_;
    int64_t ssid;
    const anjay_uri_path_t ssid_path =
            MAKE_RESOURCE_PATH(ANJAY_DM_OID_SERVER, iid,
                               ANJAY_DM_RID_SERVER_SSID);

    if (_anjay_dm_read_resource_i64(anjay, &ssid_path, &ssid)) {
        return -1;
    }
    if (ssid == (int32_t) args->ssid) {
        args->out_iid = iid;
        return ANJAY_FOREACH_BREAK;
    }
    return 0;
}

int _anjay_find_server_iid(anjay_unlocked_t *anjay,
                           anjay_ssid_t ssid,
                           anjay_iid_t *out_iid) {
    find_iid_args_t args = {
        .ssid = ssid,
        .out_iid = ANJAY_ID_INVALID
    };

    const anjay_dm_installed_object_t *obj =
            _anjay_dm_find_object_by_oid(&anjay->dm, ANJAY_DM_OID_SERVER);
    if (ssid == ANJAY_SSID_ANY || ssid == ANJAY_SSID_BOOTSTRAP
            || _anjay_dm_foreach_instance(anjay, obj, find_server_iid_handler,
                                          &args)
            || args.out_iid == ANJAY_ID_INVALID) {
        return -1;
    }
    *out_iid = args.out_iid;
    return 0;
}

bool _anjay_dm_ssid_exists(anjay_unlocked_t *anjay, anjay_ssid_t ssid) {
    assert(ssid != ANJAY_SSID_BOOTSTRAP);
    anjay_iid_t dummy_iid;
    return !_anjay_find_server_iid(anjay, ssid, &dummy_iid);
}

int _anjay_ssid_from_server_iid(anjay_unlocked_t *anjay,
                                anjay_iid_t server_iid,
                                anjay_ssid_t *out_ssid) {
    int64_t ssid;
    const anjay_uri_path_t ssid_path =
            MAKE_RESOURCE_PATH(ANJAY_DM_OID_SERVER, server_iid,
                               ANJAY_DM_RID_SERVER_SSID);
    if (_anjay_dm_read_resource_i64(anjay, &ssid_path, &ssid)) {
        return -1;
    }
    *out_ssid = (anjay_ssid_t) ssid;
    return 0;
}

int _anjay_ssid_from_security_iid(anjay_unlocked_t *anjay,
                                  anjay_iid_t security_iid,
                                  uint16_t *out_ssid) {
    assert(security_iid != ANJAY_ID_INVALID);
    if (_anjay_is_bootstrap_security_instance(anjay, security_iid)) {
        *out_ssid = ANJAY_SSID_BOOTSTRAP;
        return 0;
    }

    int64_t _ssid;
    const anjay_uri_path_t path =
            MAKE_RESOURCE_PATH(ANJAY_DM_OID_SECURITY, security_iid,
                               ANJAY_DM_RID_SECURITY_SSID);

    if (_anjay_dm_read_resource_i64(anjay, &path, &_ssid) || _ssid <= 0
            || _ssid > UINT16_MAX) {
        anjay_log(ERROR, _("could not get Short Server ID from ") "%s",
                  ANJAY_DEBUG_MAKE_PATH(&path));
        return -1;
    }

    *out_ssid = (uint16_t) _ssid;
    return 0;
}

#ifdef ANJAY_WITH_LWM2M11
int _anjay_server_uri_from_security_iid(anjay_unlocked_t *anjay,
                                        anjay_iid_t security_iid,
                                        char *out_uri,
                                        size_t out_size) {
    const anjay_uri_path_t path =
            MAKE_RESOURCE_PATH(ANJAY_DM_OID_SECURITY, security_iid,
                               ANJAY_DM_RID_SECURITY_SERVER_URI);
    if (_anjay_dm_read_resource_string(anjay, &path, out_uri, out_size)) {
        anjay_log(ERROR, _("could not get Server URI from ") "%s",
                  ANJAY_DEBUG_MAKE_PATH(&path));
        return -1;
    }
    return 0;
}
#endif // ANJAY_WITH_LWM2M11

#ifdef ANJAY_WITH_BOOTSTRAP
bool _anjay_is_bootstrap_security_instance(anjay_unlocked_t *anjay,
                                           anjay_iid_t security_iid) {
    bool is_bootstrap;
    const anjay_uri_path_t path =
            MAKE_RESOURCE_PATH(ANJAY_DM_OID_SECURITY, security_iid,
                               ANJAY_DM_RID_SECURITY_BOOTSTRAP);

    if (_anjay_dm_read_resource_bool(anjay, &path, &is_bootstrap)
            || !is_bootstrap) {
        return false;
    }

    return true;
}

static int
bootstrap_security_iid_find_helper(anjay_unlocked_t *anjay,
                                   const anjay_dm_installed_object_t *obj,
                                   anjay_iid_t iid,
                                   void *result_ptr) {
    (void) obj;
    if (_anjay_is_bootstrap_security_instance(anjay, iid)) {
        *(anjay_iid_t *) result_ptr = iid;
        return ANJAY_FOREACH_BREAK;
    }
    return ANJAY_FOREACH_CONTINUE;
}

anjay_iid_t _anjay_find_bootstrap_security_iid(anjay_unlocked_t *anjay) {
    anjay_iid_t result = ANJAY_ID_INVALID;
    const anjay_dm_installed_object_t *obj =
            _anjay_dm_find_object_by_oid(&anjay->dm, ANJAY_DM_OID_SECURITY);
    if (_anjay_dm_foreach_instance(
                anjay, obj, bootstrap_security_iid_find_helper, &result)) {
        return ANJAY_ID_INVALID;
    }
    return result;
}
#endif

avs_time_duration_t
_anjay_disable_timeout_from_server_iid(anjay_unlocked_t *anjay,
                                       anjay_iid_t server_iid) {
    static const int32_t DEFAULT_DISABLE_TIMEOUT_S = NUM_SECONDS_IN_A_DAY;

    int64_t timeout_s;
    const anjay_uri_path_t path =
            MAKE_RESOURCE_PATH(ANJAY_DM_OID_SERVER, server_iid,
                               ANJAY_DM_RID_SERVER_DISABLE_TIMEOUT);

    if (_anjay_dm_read_resource_i64(anjay, &path, &timeout_s)
            || timeout_s < 0) {
        timeout_s = DEFAULT_DISABLE_TIMEOUT_S;
    } else if (timeout_s > INT32_MAX) {
        timeout_s = INT32_MAX;
    }

    return avs_time_duration_from_scalar(timeout_s, AVS_TIME_S);
}
