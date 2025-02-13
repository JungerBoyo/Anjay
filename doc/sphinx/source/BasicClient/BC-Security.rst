..
   Copyright 2017-2024 AVSystem <avsystem@avsystem.com>
   AVSystem Anjay LwM2M SDK
   All rights reserved.

   Licensed under the AVSystem-5-clause License.
   See the attached LICENSE file for details.

Enabling secure communication
=============================

If Anjay is compiled with support for DTLS and linked with one of the
supported DTLS libraries, connection encryption is automatically handled
according to values of Resources in the Security (``/0``) Object.

This automatic configuration will be performed regardless of whether you are
using the ``security`` module that pre-implements the Security Object, or if you
perhaps decide to implement the Security Object yourself from scratch. The
library will always read the necessary DTLS configuration from the data model.

.. note:: **mbed TLS 2.0 or newer** or **OpenSSL 1.1 or newer** or
          **tinydtls 0.9 or newer** is required for proper, conformant support
          for the security modes defined in the LwM2M specification.

.. warning:: Anjay will likely compile successfully with older DTLS library
             versions, but some REQUIRED cipher suites won't be supported and
             serious interoperability problems may arise.

Supported security modes
------------------------

The security mode is determined based on the *Security Mode* Resource in a
given instance of the Security Object (``/0/*/2``). Supported values are:

* ``0`` - **Pre-Shared Key mode** - In this mode, communication is symmetrically
  encrypted and authenticated using the same secret key, shared between the
  server and the client.

  * The TLS-PSK identity is stored in the *Public Key or Identity* Resource
    (``/0/*/3``). It is a string identifying the key being used, so that the
    server can uniquely determine which key to use for communication. This
    string shall be directly stored in the aforementioned Resource.

  * The *Secret Key* (``/0/*/5``) Resource shall contain the secret pre-shared
    key itself, directly in an opaque binary format appropriate for the
    cipher suite used by the server.

* ``2`` - **Certificate mode** - In this mode, an asymmetrical public-key
  cryptographic algorithm is used to authenticate the connection endpoints and
  initialize payload encryption.

  Appropriate certificates need to be generated for both the LwM2M Client and
  the LwM2M Server. Public certificates of both parties are mutually available,
  and each party also has access to its own corresponding private key.

  * In this mode, the *Public Key or Identity* (``/0/*/3``) Resource shall
    contain the Client's public certificate in binary, DER-encoded X.509
    format.

  * The *Server Public Key* (``/0/*/4``) Resource shall contain the Server's
    public certificate, also in binary, DER-encoded X.509 format.

  * The *Secret Key* (``/0/*/5``) Resource shall contain the Client's
    private key, corresponding to the public key contained in the *Public Key or
    Identity* Resource. It needs to be in a format defined in
    `RFC 5958 <https://tools.ietf.org/html/rfc5958>`_ (also known as PKCS#8, the
    name which was used in previous versions of the format), DER-encoded into a
    binary value.

  Note that in the Certificate mode, it is not enough if the Server's
  certificate *just matches* the one stored in the *Server Public Key* resource.
  It is also verified that the certificate is issued for the same domain name
  that the Server URI points to and that it is signed by a trusted CA.

* ``3`` - **NoSec mode** - In this mode, encryption and authentication are
  disabled completely and the CoAP messages are passed in plain text over the
  network. It shall not be used in production environments, unless end-to-end
  security is provided on a lower layer (e.g. IPsec). It is also useful for
  development, testing and debugging purposes.

* ``4`` - **Certificate mode with EST** - In this mode, a certificate-based
  methods of encryption and authentication are used along with *Enrollment
  over Secure Transport* certificate management protocol. Support for this
  security mode is available as a commercial feature in Anjay. For more information,
  see the description in its :doc:`dedicated article<../CommercialFeatures/CF-EST>`.

The *Raw Public Key* mode described in the LwM2M specification is not currently
supported. 

In this tutorial, we will focus on enabling security using the PSK mode. If you
are interested in using certificates, please refer to
:doc:`../AdvancedTopics/AT-Certificates`.

Provisioning security configuration
-----------------------------------

According to the LwM2M specification, the aforementioned Resources shall be
provisioned during the Bootstrap Phase. However, if Bootstrap from Smartcard is
not used, the Client will need to contain some factory defaults for connecting
to a LwM2M Server or a LwM2M Bootstrap Server. In this section, we will learn
how to implement such factory defaults for DTLS connection.

The full code for the following example can be also found in the
``examples/tutorial/BC-Security`` directory in Anjay sources.

**Configuring encryption keys**

As mentioned above, in the case of PSK mode, the security-related data that the
LwM2M Client is operating on, is raw data. They are set using
``public_cert_or_psk_identity``, ``public_cert_or_psk_identity_size``,
``private_cert_or_psk_key`` and ``private_cert_or_psk_key_size`` fields of
``anjay_security_instance_t`` struct.

**Complete code**

Continuing the previous tutorial, we can modify ``setup_security_object()`` and
``main()`` as follows:

.. highlight:: c
.. snippet-source:: examples/tutorial/BC-Security/src/main.c
    :emphasize-lines: 14-25

    #include <anjay/anjay.h>
    #include <anjay/security.h>
    #include <anjay/server.h>
    #include <avsystem/commons/avs_log.h>

    // Installs Security Object and adds and instance of it.
    // An instance of Security Object provides information needed to connect to
    // LwM2M server.
    static int setup_security_object(anjay_t *anjay) {
        if (anjay_security_object_install(anjay)) {
            return -1;
        }

        static const char PSK_IDENTITY[] = "identity";
        static const char PSK_KEY[] = "P4s$w0rd";

        anjay_security_instance_t security_instance = {
            .ssid = 1,
            .server_uri = "coaps://eu.iot.avsystem.cloud:5684",
            .security_mode = ANJAY_SECURITY_PSK,
            .public_cert_or_psk_identity = (const uint8_t *) PSK_IDENTITY,
            .public_cert_or_psk_identity_size = strlen(PSK_IDENTITY),
            .private_cert_or_psk_key = (const uint8_t *) PSK_KEY,
            .private_cert_or_psk_key_size = strlen(PSK_KEY)
        };

        // Anjay will assign Instance ID automatically
        anjay_iid_t security_instance_id = ANJAY_ID_INVALID;
        if (anjay_security_object_add_instance(anjay, &security_instance,
                                               &security_instance_id)) {
            return -1;
        }

        return 0;
    }

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

    Complete code of this example can be found in
    `examples/tutorial/BC-Security` subdirectory of main Anjay project
    repository.

Please note, that ``server_uri`` field changed too. Now there is ``coaps``
URI scheme and port ``5684`` (default for secure CoAP).

All remaining activities related to establishing a secure communication channel
with the LwM2M Server are performed automatically by Anjay.

.. note::

    For many LwM2M Servers, including the `Coiote IoT Device Management platform
    <https://avsystem.com/coiote-iot-device-management-platform/>`_, you will
    need to change server-side configuration if you previously used NoSec
    connectivity for the same endpoint name.

    The simplest solution might often be to remove the device entry completely
    and create it from scratch.
