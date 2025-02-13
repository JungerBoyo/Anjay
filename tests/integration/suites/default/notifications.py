# -*- coding: utf-8 -*-
#
# Copyright 2017-2024 AVSystem <avsystem@avsystem.com>
# AVSystem Anjay LwM2M SDK
# All rights reserved.
#
# Licensed under the AVSystem-5-clause License.
# See the attached LICENSE file for details.
from framework.lwm2m_test import *
from framework.lwm2m.coap.transport import Transport


class CancellingConfirmableNotifications(test_suite.Lwm2mSingleServerTest,
                                         test_suite.Lwm2mDmOperations):
    def setUp(self):
        super().setUp(extra_cmdline_args=['--confirmable-notifications'])

    def runTest(self):
        self.create_instance(self.serv, oid=OID.Test, iid=0)

        observe1 = self.observe(self.serv, oid=OID.Test, iid=0, rid=RID.Test.Counter)
        observe2 = self.observe(self.serv, oid=OID.Test, iid=0, rid=RID.Test.Counter)

        self.execute_resource(self.serv, oid=OID.Test, iid=0, rid=RID.Test.IncrementCounter)
        notify1 = self.serv.recv()
        if bytes(notify1.token) != bytes(observe1.token):
            # which observation is handled first isn't exactly deterministic
            tmp = observe1
            observe1 = observe2
            observe2 = tmp

        self.assertIsInstance(notify1, Lwm2mNotify)
        self.assertEqual(bytes(notify1.token), bytes(observe1.token))
        self.serv.send(Lwm2mEmpty.matching(notify1)())

        notify2 = self.serv.recv()
        self.assertIsInstance(notify2, Lwm2mNotify)
        self.assertEqual(bytes(notify2.token), bytes(observe2.token))
        self.serv.send(Lwm2mEmpty.matching(notify2)())

        self.assertEqual(notify1.content, notify2.content)

        # Cancel the next notification
        self.execute_resource(self.serv, oid=OID.Test, iid=0, rid=RID.Test.IncrementCounter)
        notify1 = self.serv.recv()
        self.assertIsInstance(notify1, Lwm2mNotify)
        self.assertEqual(bytes(notify1.token), bytes(observe1.token))
        self.assertNotEqual(notify1.content, notify2.content)
        self.serv.send(Lwm2mReset.matching(notify1)())

        # the second observation should still produce notifications
        notify2 = self.serv.recv()
        self.assertIsInstance(notify2, Lwm2mNotify)
        self.assertEqual(bytes(notify2.token), bytes(observe2.token))
        self.assertEqual(notify1.content, notify2.content)
        self.serv.send(Lwm2mEmpty.matching(notify2)())

        self.execute_resource(self.serv, oid=OID.Test, iid=0, rid=RID.Test.IncrementCounter)
        notify2 = self.serv.recv()
        self.assertIsInstance(notify2, Lwm2mNotify)
        self.assertEqual(bytes(notify2.token), bytes(observe2.token))
        self.assertNotEqual(notify1.content, notify2.content)
        self.serv.send(Lwm2mReset.matching(notify2)())


class ClientInitiatedNotificationCancellation:
    class Test(test_suite.Lwm2mDmOperations):
        def make_cancellation_response(self, notify):
            return None

        def assertConIfUdp(self, msg):
            if self.serv.transport == Transport.UDP:
                self.assertEqual(msg.type, coap.Type.CONFIRMABLE)

        def assertNonIfUdp(self, msg):
            if self.serv.transport == Transport.UDP:
                self.assertEqual(msg.type, coap.Type.NON_CONFIRMABLE)

        def setUp(self, *args, **kwargs):
            super().setUp(*args, **kwargs)

        def runTest(self):
            observe = self.observe(self.serv, oid=OID.Location, iid=0, rid=RID.Location.Latitude)

            notify = self.serv.recv(timeout_s=2)
            self.assertIsInstance(notify, Lwm2mNotify)
            self.assertNonIfUdp(notify)
            self.assertEqual(bytes(notify.token), bytes(observe.token))
            self.assertEqual(len(notify.get_options(coap.Option.OBSERVE)), 1)

            # Unregister the Location object, this should make a read on the
            # resource to fail, which should make the client cancel the
            # observation on its initiative
            self.communicate('unregister-object %d' % OID.Location)
            self.assertDemoUpdatesRegistration(content=ANY)

            notify = self.serv.recv(timeout_s=2)
            self.assertEqual(notify.code, coap.Code.RES_NOT_FOUND)
            self.assertConIfUdp(notify)
            self.assertEqual(bytes(notify.token), bytes(observe.token))
            self.assertEqual(len(notify.get_options(coap.Option.OBSERVE)), 0)

            res = self.make_cancellation_response(notify)
            if res is not None:
                self.serv.send(res)

            # We should not receive any more notifications
            with self.assertRaises(socket.timeout):
                self.serv.recv(timeout_s=5)

    class TestWithAck(Test):
        def make_cancellation_response(self, notify):
            return Lwm2mEmpty.matching(notify)()

    # Some servers answer such a notification with a RST, not an ACK.
    class TestWithRst(Test):
        def make_cancellation_response(self, notify):
            return Lwm2mReset.matching(notify)()


class ClientInitiatedNotificationCancellationUdpAck(ClientInitiatedNotificationCancellation.TestWithAck,
                                                    test_suite.Lwm2mSingleServerTest):
    pass


class ClientInitiatedNotificationCancellationDtlsAck(ClientInitiatedNotificationCancellation.TestWithAck,
                                                    test_suite.Lwm2mDtlsSingleServerTest):
    pass


class ClientInitiatedNotificationCancellationUdpRst(ClientInitiatedNotificationCancellation.TestWithRst,
                                                    test_suite.Lwm2mSingleServerTest):
    pass


class ClientInitiatedNotificationCancellationDtlsRst(ClientInitiatedNotificationCancellation.TestWithRst,
                                                    test_suite.Lwm2mDtlsSingleServerTest):
    pass


class ClientInitiatedNotificationCancellationTcp(ClientInitiatedNotificationCancellation.Test,
                                                    test_suite.Lwm2mSingleTcpServerTest):
    def setUp(self, *args, **kwargs):
        super().setUp(binding='T', *args, **kwargs)


class ClientInitiatedNotificationCancellationTls(ClientInitiatedNotificationCancellation.Test,
                                                    test_suite.Lwm2mTlsSingleServerTest):
    def setUp(self, *args, **kwargs):
        super().setUp(binding='T', *args, **kwargs)


class SelfNotifyDisabled(test_suite.Lwm2mSingleServerTest, test_suite.Lwm2mDmOperations):
    def runTest(self):
        self.create_instance(self.serv, oid=OID.Test, iid=42)

        self.observe(self.serv, oid=OID.Test, iid=42, rid=RID.Test.ResInt)

        self.write_resource(self.serv, oid=OID.Test, iid=42, rid=RID.Test.ResInt, content=b'42')
        # no notification expected in this case
        with self.assertRaises(socket.timeout):
            self.serv.recv(timeout_s=3)


class SelfNotifyEnabled(test_suite.Lwm2mSingleServerTest, test_suite.Lwm2mDmOperations):
    def setUp(self):
        super().setUp(extra_cmdline_args=['--enable-self-notify'])

    def runTest(self):
        self.create_instance(self.serv, oid=OID.Test, iid=42)

        observe = self.observe(self.serv, oid=OID.Test, iid=42, rid=RID.Test.ResInt)

        self.write_resource(self.serv, oid=OID.Test, iid=42, rid=RID.Test.ResInt, content=b'42')
        notify = self.serv.recv()
        self.assertIsInstance(notify, Lwm2mNotify)
        self.assertEqual(bytes(notify.token), bytes(observe.token))
        self.assertEqual(notify.content, b'42')


class RegisterCancelsNotifications(test_suite.Lwm2mSingleServerTest, test_suite.Lwm2mDmOperations):
    def runTest(self):
        self.create_instance(self.serv, oid=OID.Test, iid=1)
        self.create_instance(self.serv, oid=OID.Test, iid=2)

        self.observe(self.serv, oid=OID.Test, iid=1, rid=RID.Test.Counter)
        self.execute_resource(self.serv, oid=OID.Test, iid=1, rid=RID.Test.IncrementCounter)
        self.assertIsInstance(self.serv.recv(), Lwm2mNotify)

        self.observe(self.serv, oid=OID.Test, iid=2, rid=RID.Test.Counter)
        self.execute_resource(self.serv, oid=OID.Test, iid=2, rid=RID.Test.IncrementCounter)
        self.assertIsInstance(self.serv.recv(), Lwm2mNotify)

        self.communicate('send-update')
        update = self.assertDemoUpdatesRegistration(respond=False, content=ANY)

        # Notifications shall work while Updating
        self.execute_resource(self.serv, oid=OID.Test, iid=1, rid=RID.Test.IncrementCounter)
        self.execute_resource(self.serv, oid=OID.Test, iid=2, rid=RID.Test.IncrementCounter)
        notifications = 0
        while True:
            pkt = self.serv.recv()
            if notifications < 2 and isinstance(pkt, Lwm2mNotify):
                notifications += 1
                if notifications == 2:
                    break
            else:
                self.assertMsgEqual(pkt, update)

        # force a Register
        self.serv.send(Lwm2mErrorResponse.matching(update)(coap.Code.RES_FORBIDDEN))
        register = self.assertDemoRegisters(respond=False)

        # Notifications shall no longer work here
        self.execute_resource(self.serv, oid=OID.Test, iid=1, rid=RID.Test.IncrementCounter)
        self.execute_resource(self.serv, oid=OID.Test, iid=2, rid=RID.Test.IncrementCounter)
        pkt = self.serv.recv()
        self.assertMsgEqual(pkt, register)

        self.serv.send(Lwm2mCreated.matching(pkt)(location=self.DEFAULT_REGISTER_ENDPOINT))

        # Check that notifications still don't work
        self.execute_resource(self.serv, oid=OID.Test, iid=1, rid=RID.Test.IncrementCounter)
        self.execute_resource(self.serv, oid=OID.Test, iid=2, rid=RID.Test.IncrementCounter)
        with self.assertRaises(socket.timeout):
            print(self.serv.recv(timeout_s=5))


class LifetimeExpirationCancelsObserveWhileStoring(test_suite.Lwm2mDtlsSingleServerTest,
                                                   test_suite.Lwm2mDmOperations):
    def setUp(self):
        super().setUp(lifetime=30)

    def runTest(self):
        expiration_time = float(self.communicate('registration-expiration-time 1',
                                                 match_regex='REGISTRATION_EXPIRATION_TIME=(.*)\n').group(
            1))

        observe = self.observe(self.serv, oid=OID.Device, iid=0, rid=RID.Device.CurrentTime)
        observe_start_time = time.time()

        notifications_generated = 0
        self.communicate('enter-offline')
        # Receive the notifications that might have managed to be sent before entering offline mode
        deadline = time.time() + 5
        while time.time() < deadline:
            try:
                self.assertIsInstance(self.serv.recv(deadline=deadline), Lwm2mNotify)
                notifications_generated += 1
            except socket.timeout:
                break

        # Check that the notifications are stored alright during offline mode
        deadline = expiration_time + 5
        while True:
            timeout_s = max(0.0, deadline - time.time())
            if self.read_log_until_match(
                    b'Notify for token %s scheduled:' % (binascii.hexlify(observe.token),),
                    timeout_s=timeout_s) is not None:
                notifications_generated += 1
            else:
                break

        # Assert that the notification has been implicitly cancelled
        self.assertIsNotNone(
            self.read_log_until_match(b'Observe cancel: %s\n' % (binascii.hexlify(observe.token),),
                                      timeout_s=5))

        self.assertAlmostEqual(notifications_generated, expiration_time - observe_start_time,
                               delta=2)

        self.communicate('exit-offline')

        # The client will register again
        self.assertDtlsReconnect()
        self.assertDemoRegisters(lifetime=30)
        # Check that no notifications are sent
        with self.assertRaises(socket.timeout):
            print(self.serv.recv(timeout_s=5))


class UdpNotificationErrorTest(test_suite.Lwm2mSingleServerTest, test_suite.Lwm2mDmOperations):
    def setUp(self):
        super().setUp(extra_cmdline_args=['--confirmable-notifications'])

    def runTest(self):
        self.write_resource(self.serv, oid=OID.Server, iid=1, rid=RID.Server.DefaultMaxPeriod,
                            content=b'2')

        self.create_instance(self.serv, oid=OID.Test, iid=1)
        orig_notif = self.observe(self.serv, oid=OID.Test, iid=1, rid=RID.Test.Counter)
        self.delete_instance(self.serv, oid=OID.Test, iid=1)

        notif = self.serv.recv(timeout_s=5)
        self.assertEqual(notif.code, coap.Code.RES_NOT_FOUND)
        self.assertEqual(notif.token, orig_notif.token)
        self.serv.send(Lwm2mEmpty.matching(notif)())


class TcpNotificationErrorTest(test_suite.Lwm2mSingleTcpServerTest, test_suite.Lwm2mDmOperations):
    def setUp(self):
        super().setUp(extra_cmdline_args=['--confirmable-notifications'], binding='T')

    def runTest(self):
        self.write_resource(self.serv, oid=OID.Server, iid=1, rid=RID.Server.DefaultMaxPeriod,
                            content=b'2')

        self.create_instance(self.serv, oid=OID.Test, iid=1)
        orig_notif = self.observe(self.serv, oid=OID.Test, iid=1, rid=RID.Test.Counter)
        self.delete_instance(self.serv, oid=OID.Test, iid=1)

        notif = self.serv.recv(timeout_s=5)
        self.assertEqual(notif.code, coap.Code.RES_NOT_FOUND)
        self.assertEqual(notif.token, orig_notif.token)


class NextPlannedNotify(test_suite.Lwm2mSingleServerTest, test_suite.Lwm2mDmOperations):
    def setUp(self):
        super().setUp()

    def runTest(self):
        # set up observe without attributes
        self.observe(self.serv, oid=OID.Device, iid=0, rid=RID.Device.ErrorCode)

        # ensure there is no planned notify
        self.communicate('next-planned-pmax-notify',
                         match_regex='NEXT_PLANNED_PMAX_NOTIFY=TIME_INVALID\n')
        self.communicate('next-planned-notify',
                         match_regex='NEXT_PLANNED_NOTIFY=TIME_INVALID\n')
        
        # add an observation with pmin&pmax
        write_atts = self.write_attributes(self.serv, oid=OID.Device, iid=0, rid=RID.Device.Timezone,
                            query=['pmin=5','pmax=5'])
        self.assertEqual(write_atts.code, coap.Code.RES_CHANGED)

        second_notif = self.observe(self.serv, oid=OID.Device, iid=0, rid=RID.Device.Timezone)
        self.assertEqual(second_notif.code, coap.Code.RES_CONTENT)

        expected_notify_time = time.time() + 5.0

        time.sleep(0.1)

        # verify that Anjay schedules the notifications
        planned_notify_time = float(
            self.communicate('next-planned-notify',
                             match_regex='NEXT_PLANNED_NOTIFY=(.*)\n').group(1))
        self.assertAlmostEqual(expected_notify_time, planned_notify_time, delta=0.1)

        # wait for the notification and check if next-planned-notify was rescheduled
        notif = self.serv.recv(timeout_s=6)
        self.assertEqual(notif.code, coap.Code.RES_CONTENT)
        self.assertEqual(notif.token, second_notif.token)

        expected_notify_time = expected_notify_time + 5
        planned_notify_time = float(
            self.communicate('next-planned-notify',
                             match_regex='NEXT_PLANNED_NOTIFY=(.*)\n').group(1))
        self.assertAlmostEqual(expected_notify_time, planned_notify_time, delta=0.1)

        # cancel the observation
        self.observe(self.serv, oid=OID.Device, iid=0, rid=RID.Device.Timezone, observe=1, token=notif.token)
        time.sleep(0.2)

        # ensure there is no planned notify
        self.communicate('next-planned-pmax-notify',
                         match_regex='NEXT_PLANNED_PMAX_NOTIFY=TIME_INVALID\n')
        self.communicate('next-planned-notify',
                         match_regex='NEXT_PLANNED_NOTIFY=TIME_INVALID\n')
