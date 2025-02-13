..
   Copyright 2017-2024 AVSystem <avsystem@avsystem.com>
   AVSystem Anjay LwM2M SDK
   All rights reserved.

   Licensed under the AVSystem-5-clause License.
   See the attached LICENSE file for details.

Installing mandatory Objects
============================

In order to be able to connect to some LwM2M Server and handle incoming
packets our client has to have at least `LwM2M Security
<https://www.openmobilealliance.org/tech/profiles/LWM2M_Security-v1_0.xml>`_
(``/0``) and `LwM2M Server
<https://www.openmobilealliance.org/tech/profiles/LWM2M_Server-v1_0.xml>`_
(``/1``) Objects implemented.

Fortunately, Anjay provides both of these Objects in the form of pre-implemented
modules, and they can be used easily.

.. note::
    It doesn't impact on Anjay flexibility -- i.e., users can still provide
    their own implementation of these Objects if necessary.

When Anjay is first instantiated (as in our previous :ref:`hello world
<anjay-hello-world>` example), it has no knowledge about the Data Model,
i.e., no LwM2M Objects are registered within it. Security and Server objects can
be registered using installation mechanism, presented in the next subsection.

Installing Objects
^^^^^^^^^^^^^^^^^^

Each LwM2M Object is defined by an instance of the ``anjay_dm_object_def_t``
structure. To add support for a new Object, you'd need to:

  - fill the ``anjay_dm_object_def_t`` structure,
  - implement appropriate callback functions,
  - register created object in Anjay.

However, for now, we are going to install our pre-implemented LwM2M Objects
(Security, Server), so that you don't have to worry about initializing the
structure and object registration on your own. In case you are interested in
this topic, :doc:`BC-ObjectImplementation` section provides more information on
this subject.

To install the Objects we are going to use ``anjay_security_object_install()``
and ``anjay_server_object_install()`` functions.

.. important::

    Remember to include ``anjay/security.h`` and ``anjay/server.h`` headers to
    use the functions mentioned above.

Setting up Server and Security Objects
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Now we are going to create functions which install Server and Security Objects
and add instances of them. We modify the code from the
:ref:`previous tutorial <anjay-hello-world>`.

The first one will be ``setup_security_object()``. In this tutorial, we will use
the Coiote IoT Device Management platform as the hard-coded server URI. You can
go to https://avsystem.com/coiote-iot-device-management-platform/ to create an
account, and after logging in, add the device entry for your application. If you
wish to use another server, then you must replace
``coap://eu.iot.avsystem.cloud:5683`` with a valid value.

For now, we will establish non-secure connection, a secure one will be described
later.

.. highlight:: c
.. snippet-source:: examples/tutorial/BC-MandatoryObjects/src/main.c

    // Installs Security Object and adds and instance of it.
    // An instance of Security Object provides information needed to connect to
    // LwM2M server.
    static int setup_security_object(anjay_t *anjay) {
        if (anjay_security_object_install(anjay)) {
            return -1;
        }

        const anjay_security_instance_t security_instance = {
            .ssid = 1,
            .server_uri = "coap://eu.iot.avsystem.cloud:5683",
            .security_mode = ANJAY_SECURITY_NOSEC
        };

        // Anjay will assign Instance ID automatically
        anjay_iid_t security_instance_id = ANJAY_ID_INVALID;
        if (anjay_security_object_add_instance(anjay, &security_instance,
                                               &security_instance_id)) {
            return -1;
        }

        return 0;
    }

Both Security and Server instances are linked together by the Short Server ID
Resource (``ssid``). That is why we keep SSID matched in both
``setup_server_object()`` and ``setup_security_object()``.

.. highlight:: c
.. snippet-source:: examples/tutorial/BC-MandatoryObjects/src/main.c

    // Installs Server Object and adds and instance of it.
    // An instance of Server Object provides the data related to a LwM2M Server.
    static int setup_server_object(anjay_t *anjay) {
        if (anjay_server_object_install(anjay)) {
            return -1;
        }

        const anjay_server_instance_t server_instance = {
            // Server Short ID
            .ssid = 1,
            // Client will send Update message often than every 60 seconds
            .lifetime = 60,
            // Disable Default Minimum Period resource
            .default_min_period = -1,
            // Disable Default Maximum Period resource
            .default_max_period = -1,
            // Disable Disable Timeout resource
            .disable_timeout = -1,
            // Sets preferred transport to UDP
            .binding = "U"
        };

        // Anjay will assign Instance ID automatically
        anjay_iid_t server_instance_id = ANJAY_ID_INVALID;
        if (anjay_server_object_add_instance(anjay, &server_instance,
                                             &server_instance_id)) {
            return -1;
        }

        return 0;
    }

Now we are ready to call these functions from ``main()``.

.. highlight:: c
.. snippet-source:: examples/tutorial/BC-MandatoryObjects/src/main.c
    :emphasize-lines: 21-24

    int main(int argc, char *argv[]) {
        if (argc != 2) {
            avs_log(tutorial, ERROR, "usage: %s ENDPOINT_NAME", argv[0]);
            return -1;
        }

        const anjay_configuration_t CONFIG = {
            .endpoint_name = argv[1],
            .in_buffer_size = 4000,
            .out_buffer_size = 4000,
            .msg_cache_size = 4000
        };

        anjay_t *anjay = anjay_new(&CONFIG);
        if (!anjay) {
            avs_log(tutorial, ERROR, "Could not create Anjay object");
            return -1;
        }

        int result = 0;
        // Setup necessary objects
        if (setup_security_object(anjay) || setup_server_object(anjay)) {
            result = -1;
        }

        if (!result) {
            result = anjay_event_loop_run(
                    anjay, avs_time_duration_from_scalar(1, AVS_TIME_S));
        }

        anjay_delete(anjay);
        return result;
    }

.. note::

    ``anjay_delete()`` will automatically delete installed modules after
    destruction of Anjay instance.

.. note::

    Complete code of this example can be found in
    `examples/tutorial/BC-MandatoryObjects` subdirectory of main Anjay project
    repository.

After running the client, you should see ``registration successful, location =
/rd/<server-dependent identifier>`` once and ``registration successfully
updated`` every 30 seconds in logs. It means, that the client has connected to
the server and successfully sends Update messages. Now you can perform some
Reads for example from the LwM2M Server side.

Application events
^^^^^^^^^^^^^^^^^^

The code above handles all events that may happen within the Anjay library
itself. Of course, the application usually needs to handle its own
functionality. Some ways to do this will be handled later in the
:doc:`BC-Notifications` tutorial.
