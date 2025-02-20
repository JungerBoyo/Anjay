/*
 * Copyright 2017-2024 AVSystem <avsystem@avsystem.com>
 * AVSystem Anjay LwM2M SDK
 * All rights reserved.
 *
 * Licensed under the AVSystem-5-clause License.
 * See the attached LICENSE file for details.
 */
#include <anjay_init.h>

#ifdef ANJAY_WITH_MODULE_IPSO_OBJECTS

#    include <assert.h>
#    include <stdbool.h>

#    include <anjay/anjay.h>
#    include <anjay/dm.h>
#    include <anjay/ipso_objects.h>

#    include <anjay_modules/anjay_dm_utils.h>
#    include <anjay_modules/anjay_utils_core.h>

#    include <avsystem/commons/avs_defs.h>
#    include <avsystem/commons/avs_log.h>
#    include <avsystem/commons/avs_memory.h>

VISIBILITY_SOURCE_BEGIN

/**
 * Min Range Value: R, Single, Optional
 * type: float, range: N/A, unit: N/A
 * The minimum value that can be measured the sensor.
 */
#    define RID_MIN_RANGE_VALUE 5603

/**
 * Max Range Value: R, Single, Optional
 * type: float, range: N/A, unit: N/A
 * The maximum value that can be measured by the sensor.
 */
#    define RID_MAX_RANGE_VALUE 5604

/**
 * Sensor Units: R, Single, Optional
 * type: string, range: N/A, unit: N/A
 * Measurement Units Definition.
 */
#    define RID_SENSOR_UNITS 5701

/**
 * X Value: R, Single, Mandatory
 * type: float, range: N/A, unit: N/A
 * The measured value along the X axis.
 */
#    define RID_X_VALUE 5702

/**
 * Y Value: R, Single, Optional
 * type: float, range: N/A, unit: N/A
 * The measured value along the Y axis.
 */
#    define RID_Y_VALUE 5703

/**
 * Z Value: R, Single, Optional
 * type: float, range: N/A, unit: N/A
 * The measured value along the Z axis.
 */
#    define RID_Z_VALUE 5704

typedef struct {
    anjay_ipso_3d_sensor_impl_t impl;
    bool initialized;

    double x_value;
    double y_value;
    double z_value;
} anjay_ipso_3d_sensor_instance_t;

typedef struct {
    anjay_dm_installed_object_t obj_def_ptr;
    const anjay_unlocked_dm_object_def_t *obj_def;
    anjay_unlocked_dm_object_def_t def;

    size_t num_instances;
    anjay_ipso_3d_sensor_instance_t instances[];
} anjay_ipso_3d_sensor_t;

static anjay_ipso_3d_sensor_t *
get_obj(const anjay_dm_installed_object_t *obj_ptr) {
    assert(obj_ptr);
    return AVS_CONTAINER_OF(_anjay_dm_installed_object_get_unlocked(obj_ptr),
                            anjay_ipso_3d_sensor_t, obj_def);
}

static int
ipso_3d_sensor_list_instances(anjay_unlocked_t *anjay,
                              const anjay_dm_installed_object_t obj_ptr,
                              anjay_unlocked_dm_list_ctx_t *ctx) {
    (void) anjay;

    anjay_ipso_3d_sensor_t *obj = get_obj(&obj_ptr);

    for (anjay_iid_t iid = 0; iid < obj->num_instances; iid++) {
        if (obj->instances[iid].initialized) {
            _anjay_dm_emit_unlocked(ctx, iid);
        }
    }

    return 0;
}

static int
ipso_3d_sensor_list_resources(anjay_unlocked_t *anjay,
                              const anjay_dm_installed_object_t obj_ptr,
                              anjay_iid_t iid,
                              anjay_unlocked_dm_resource_list_ctx_t *ctx) {
    (void) anjay;

    anjay_ipso_3d_sensor_t *obj = get_obj(&obj_ptr);
    assert(iid < obj->num_instances);
    anjay_ipso_3d_sensor_instance_t *inst = &obj->instances[iid];
    assert(inst->initialized);

    if (!isnan(inst->impl.min_range_value)) {
        _anjay_dm_emit_res_unlocked(ctx, RID_MIN_RANGE_VALUE, ANJAY_DM_RES_R,
                                    ANJAY_DM_RES_PRESENT);
    }
    if (!isnan(inst->impl.max_range_value)) {
        _anjay_dm_emit_res_unlocked(ctx, RID_MAX_RANGE_VALUE, ANJAY_DM_RES_R,
                                    ANJAY_DM_RES_PRESENT);
    }
    _anjay_dm_emit_res_unlocked(ctx, RID_SENSOR_UNITS, ANJAY_DM_RES_R,
                                ANJAY_DM_RES_PRESENT);
    _anjay_dm_emit_res_unlocked(ctx, RID_X_VALUE, ANJAY_DM_RES_R,
                                ANJAY_DM_RES_PRESENT);
    if (inst->impl.use_y_value) {
        _anjay_dm_emit_res_unlocked(ctx, RID_Y_VALUE, ANJAY_DM_RES_R,
                                    ANJAY_DM_RES_PRESENT);
    }
    if (inst->impl.use_z_value) {
        _anjay_dm_emit_res_unlocked(ctx, RID_Z_VALUE, ANJAY_DM_RES_R,
                                    ANJAY_DM_RES_PRESENT);
    }

    return 0;
}

static int update_values(anjay_unlocked_t *anjay,
                         anjay_oid_t oid,
                         anjay_iid_t iid,
                         anjay_ipso_3d_sensor_instance_t *inst) {
    double x_value = NAN, y_value = NAN, z_value = NAN;
    int err = -1;
    ANJAY_MUTEX_UNLOCK_FOR_CALLBACK(anjay_locked, anjay);
    err = inst->impl.get_values(iid, inst->impl.user_context, &x_value,
                                &y_value, &z_value);
    ANJAY_MUTEX_LOCK_AFTER_CALLBACK(anjay_locked);

    if (err) {
        return err;
    }

    if (x_value != inst->x_value) {
        inst->x_value = x_value;
        (void) _anjay_notify_changed_unlocked(anjay, oid, iid, RID_X_VALUE);
    }

    if (inst->impl.use_y_value && y_value != inst->y_value) {
        inst->y_value = y_value;
        (void) _anjay_notify_changed_unlocked(anjay, oid, iid, RID_Y_VALUE);
    }

    if (inst->impl.use_z_value && z_value != inst->z_value) {
        inst->z_value = z_value;
        (void) _anjay_notify_changed_unlocked(anjay, oid, iid, RID_Z_VALUE);
    }

    return 0;
}

static int
ipso_3d_sensor_resource_read(anjay_unlocked_t *anjay,
                             const anjay_dm_installed_object_t obj_ptr,
                             anjay_iid_t iid,
                             anjay_rid_t rid,
                             anjay_riid_t riid,
                             anjay_unlocked_output_ctx_t *ctx) {
    (void) anjay;
    (void) riid;

    anjay_ipso_3d_sensor_t *obj = get_obj(&obj_ptr);
    assert(iid < obj->num_instances);
    anjay_ipso_3d_sensor_instance_t *inst = &obj->instances[iid];
    assert(inst->initialized);

    switch (rid) {
    case RID_SENSOR_UNITS:
        assert(riid == ANJAY_ID_INVALID);
        return _anjay_ret_string_unlocked(ctx, inst->impl.unit);

    case RID_X_VALUE:
        assert(riid == ANJAY_ID_INVALID);
        (void) update_values(anjay, obj->def.oid, iid, inst);
        return _anjay_ret_double_unlocked(ctx, inst->x_value);

    case RID_Y_VALUE:
        assert(riid == ANJAY_ID_INVALID);
        if (inst->impl.use_y_value) {
            (void) update_values(anjay, obj->def.oid, iid, inst);
            return _anjay_ret_double_unlocked(ctx, inst->y_value);
        } else {
            return ANJAY_ERR_METHOD_NOT_ALLOWED;
        }

    case RID_Z_VALUE:
        assert(riid == ANJAY_ID_INVALID);
        if (inst->impl.use_z_value) {
            (void) update_values(anjay, obj->def.oid, iid, inst);
            return _anjay_ret_double_unlocked(ctx, inst->z_value);
        } else {
            return ANJAY_ERR_METHOD_NOT_ALLOWED;
        }

    case RID_MIN_RANGE_VALUE:
        assert(riid == ANJAY_ID_INVALID);
        if (isnan(inst->impl.min_range_value)) {
            return ANJAY_ERR_METHOD_NOT_ALLOWED;
        } else {
            return _anjay_ret_double_unlocked(ctx, inst->impl.min_range_value);
        }

    case RID_MAX_RANGE_VALUE:
        assert(riid == ANJAY_ID_INVALID);
        if (isnan(inst->impl.max_range_value)) {
            return ANJAY_ERR_METHOD_NOT_ALLOWED;
        } else {
            return _anjay_ret_double_unlocked(ctx, inst->impl.max_range_value);
        }

    default:
        return ANJAY_ERR_METHOD_NOT_ALLOWED;
    }
}

static anjay_ipso_3d_sensor_t *obj_from_oid(anjay_unlocked_t *anjay,
                                            anjay_oid_t oid) {
    const anjay_dm_installed_object_t *installed_obj_ptr =
            _anjay_dm_find_object_by_oid(_anjay_get_dm(anjay), oid);
    if (!_anjay_dm_installed_object_is_valid_unlocked(installed_obj_ptr)) {
        return NULL;
    }

    // Checks if it is really an instance of anjay_ipso_3d_sensor_t
    return (*_anjay_dm_installed_object_get_unlocked(installed_obj_ptr))
                                   ->handlers.list_instances
                           == ipso_3d_sensor_list_instances
                   ? get_obj(installed_obj_ptr)
                   : NULL;
}

int anjay_ipso_3d_sensor_install(anjay_t *anjay_locked,
                                 anjay_oid_t oid,
                                 size_t num_instances) {
    if (!anjay_locked) {
        _anjay_log(ipso, ERROR, _("Anjay pointer is NULL"));
        return -1;
    }

    int err = 0;
    ANJAY_MUTEX_LOCK(anjay, anjay_locked);

    AVS_LIST(anjay_dm_installed_object_t) entry;
    anjay_ipso_3d_sensor_t *obj =
            (anjay_ipso_3d_sensor_t *) AVS_LIST_NEW_BUFFER(
                    sizeof(anjay_ipso_3d_sensor_t)
                    + num_instances * sizeof(anjay_ipso_3d_sensor_instance_t));
    if (!obj) {
        _anjay_log_oom();
        err = -1;
        goto finish;
    }

    obj->def = (anjay_unlocked_dm_object_def_t) {
        .oid = oid,
        .handlers = {
            .list_instances = ipso_3d_sensor_list_instances,
            .list_resources = ipso_3d_sensor_list_resources,
            .resource_read = ipso_3d_sensor_resource_read,
        }
    };

    obj->obj_def = &obj->def;
    obj->num_instances = num_instances;

    _anjay_dm_installed_object_init_unlocked(&obj->obj_def_ptr, &obj->obj_def);
    _ANJAY_ASSERT_INSTALLED_OBJECT_IS_FIRST_FIELD(anjay_ipso_3d_sensor_t,
                                                  obj_def_ptr);
    entry = &obj->obj_def_ptr;
    if (_anjay_register_object_unlocked(anjay, &entry)) {
        avs_free(obj);
        err = -1;
    }

finish:;
    ANJAY_MUTEX_UNLOCK(anjay_locked);
    return err;
}

int anjay_ipso_3d_sensor_instance_add(anjay_t *anjay_locked,
                                      anjay_oid_t oid,
                                      anjay_iid_t iid,
                                      anjay_ipso_3d_sensor_impl_t impl) {
    if (!anjay_locked) {
        _anjay_log(ipso, ERROR, _("Anjay pointer is NULL"));
        return -1;
    }

    int err = 0;
    ANJAY_MUTEX_LOCK(anjay, anjay_locked);

    anjay_ipso_3d_sensor_instance_t *inst;
    anjay_ipso_3d_sensor_t *obj = obj_from_oid(anjay, oid);
    if (!obj) {
        _anjay_log(ipso, ERROR, _("Object") " %d" _(" not installed"), oid);
        err = -1;
        goto finish;
    }

    if (iid >= obj->num_instances) {
        _anjay_log(ipso, ERROR, _("IID too large"));
        err = -1;
        goto finish;
    }

    if (!impl.get_values) {
        _anjay_log(ipso, ERROR, _("Callback is NULL"));
        goto finish;
    }

    double x_value, y_value, z_value;
    ANJAY_MUTEX_UNLOCK_FOR_CALLBACK(anjay_locked_2, anjay);
    if (impl.get_values(iid, impl.user_context, &x_value, &y_value, &z_value)) {
        _anjay_log(ipso, WARNING, _("Read of") " /%d/%d" _(" failed"), oid,
                   iid);
        x_value = NAN;
        y_value = NAN;
        z_value = NAN;
    }
    ANJAY_MUTEX_LOCK_AFTER_CALLBACK(anjay_locked_2);

    inst = &obj->instances[iid];

    inst->initialized = true;
    inst->impl = impl;
    inst->x_value = x_value;
    if (inst->impl.use_y_value) {
        inst->y_value = y_value;
    }
    if (inst->impl.use_z_value) {
        inst->z_value = z_value;
    }

    _anjay_notify_instances_changed_unlocked(anjay, oid);

    (void) _anjay_notify_changed_unlocked(anjay, oid, iid, RID_X_VALUE);
    if (inst->impl.use_y_value) {
        (void) _anjay_notify_changed_unlocked(anjay, oid, iid, RID_Y_VALUE);
    }
    if (inst->impl.use_z_value) {
        (void) _anjay_notify_changed_unlocked(anjay, oid, iid, RID_Z_VALUE);
    }

finish:;
    ANJAY_MUTEX_UNLOCK(anjay_locked);
    return err;
}

int anjay_ipso_3d_sensor_instance_remove(anjay_t *anjay_locked,
                                         anjay_oid_t oid,
                                         anjay_iid_t iid) {
    if (!anjay_locked) {
        _anjay_log(ipso, ERROR, _("Anjay pointer is NULL"));
        return -1;
    }

    int err = 0;
    ANJAY_MUTEX_LOCK(anjay, anjay_locked);

    anjay_ipso_3d_sensor_t *obj = obj_from_oid(anjay, oid);
    if (!obj) {
        _anjay_log(ipso, ERROR, _("Object") " %d" _(" not installed"), oid);
        err = -1;
        goto finish;
    }

    if (iid >= obj->num_instances || !obj->instances[iid].initialized) {
        _anjay_log(ipso, ERROR, _("Object") " %d" _(" has no instance") " %d",
                   oid, iid);
        err = -1;
    } else {
        obj->instances[iid].initialized = false;
    }

    _anjay_notify_instances_changed_unlocked(anjay, oid);

finish:;
    ANJAY_MUTEX_UNLOCK(anjay_locked);
    return err;
}

int anjay_ipso_3d_sensor_update(anjay_t *anjay_locked,
                                anjay_oid_t oid,
                                anjay_iid_t iid) {
    if (!anjay_locked) {
        _anjay_log(ipso, ERROR, _("Anjay pointer is NULL"));
        return -1;
    }

    int err = 0;
    ANJAY_MUTEX_LOCK(anjay, anjay_locked);

    anjay_ipso_3d_sensor_t *obj = obj_from_oid(anjay, oid);
    if (!obj) {
        _anjay_log(ipso, ERROR, _("Object") " %d" _(" not installed"), oid);
        err = -1;
        goto finish;
    }

    anjay_ipso_3d_sensor_instance_t *inst;
    if (iid > obj->num_instances
            || !(inst = &obj->instances[iid])->initialized) {
        _anjay_log(ipso, ERROR, _("Object") " %d" _(" has no instance") " %d",
                   oid, iid);
        err = -1;
        goto finish;
    }

    if (update_values(anjay, oid, iid, inst)) {
        _anjay_log(ipso, WARNING, _("Update of") " /%d/%d" _(" failed"), oid,
                   iid);
        err = -1;
    }

finish:;
    ANJAY_MUTEX_UNLOCK(anjay_locked);
    return err;
}

#endif // ANJAY_WITH_MODULE_IPSO_OBJECTS
