#!/usr/bin/env python3
"""Tests for the USB network link and the SSH socket on it.

The helper runs as a real bash process against a temporary directory that
stands in for a configfs gadget, with a fake device-env; no gadget, network or
systemd state is touched. The unit files are checked for the properties the
design depends on. Standard library only; run with python3 -m unittest.
"""

import configparser
import os
import subprocess
import tempfile
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SYSTEM = os.path.join(ROOT, "system_files")
HELPER = os.path.join(SYSTEM, "usr", "libexec", "armada", "usb-network")
UNITS = os.path.join(SYSTEM, "usr", "lib", "systemd", "system")
CONTROL = os.path.join(SYSTEM, "usr", "libexec", "armada", "armada-control")
DEVICES = os.path.join(SYSTEM, "usr", "lib", "armada", "devices")
NM_KEYFILE = os.path.join(
    SYSTEM, "etc", "NetworkManager", "system-connections", "armada-usb.nmconnection"
)


def unit(*parts):
    parser = configparser.ConfigParser(strict=False, interpolation=None)
    parser.optionxform = str
    with open(os.path.join(UNITS, *parts), encoding="utf-8") as handle:
        parser.read_string(handle.read())
    return parser


def raw(*parts):
    with open(os.path.join(UNITS, *parts), encoding="utf-8") as handle:
        return handle.read()


class HelperTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.gadget = os.path.join(self.tmp.name, "armada-mtp")
        os.makedirs(os.path.join(self.gadget, "configs", "c.1"))
        os.makedirs(os.path.join(self.gadget, "functions"))

        self.config = os.path.join(self.tmp.name, "etc", "usb-network.conf")
        # A fake systemctl on PATH logs every call; is-active answers from MTP_ACTIVE.
        self.bin = os.path.join(self.tmp.name, "bin")
        os.makedirs(self.bin)
        self.calls = os.path.join(self.tmp.name, "systemctl.log")
        systemctl = os.path.join(self.bin, "systemctl")
        with open(systemctl, "w", encoding="utf-8") as handle:
            handle.write(
                "#!/bin/sh\n"
                f'echo "$*" >> "{self.calls}"\n'
                'case "$1" in is-active) [ "$MTP_ACTIVE" = 1 ] && [ "$3" = armada-mtp.service ] ;;\n'
                "is-system-running) echo running ;; esac\n"
            )
        os.chmod(systemctl, 0o755)

    def run_helper(self, *args, usb_network="1", mtp_active=False):
        env_script = os.path.join(self.tmp.name, "device-env")
        with open(env_script, "w", encoding="utf-8") as handle:
            handle.write(f"#!/bin/sh\necho ARMADA_USB_NETWORK={usb_network}\n")
        os.chmod(env_script, 0o755)
        env = dict(
            os.environ,
            ARMADA_DEVICE_ENV=env_script,
            ARMADA_USB_NETWORK_CONFIG=self.config,
            MTP_ACTIVE="1" if mtp_active else "0",
            PATH=self.bin + os.pathsep + os.environ["PATH"],
        )
        return subprocess.run(
            ["bash", HELPER, *args], env=env, capture_output=True, text=True, check=False
        )

    def systemctl_calls(self):
        if not os.path.exists(self.calls):
            return []
        with open(self.calls, encoding="utf-8") as handle:
            return [line for line in handle.read().splitlines() if not line.startswith("is-")]

    def write_switch(self, text):
        os.makedirs(os.path.dirname(self.config), exist_ok=True)
        with open(self.config, "w", encoding="utf-8") as handle:
            handle.write(text)

    def test_switch_is_on_without_a_config(self):
        self.assertEqual(self.run_helper("enabled").returncode, 0)
        self.assertEqual(self.run_helper("state").stdout, "enabled=1\nsupported=1\n")

    def test_switch_off_disables_the_link(self):
        self.write_switch("enabled=0\n")
        self.assertNotEqual(self.run_helper("enabled").returncode, 0)
        self.assertEqual(self.run_helper("state").stdout, "enabled=0\nsupported=1\n")
        self.write_switch("enabled=1\n")
        self.assertEqual(self.run_helper("enabled").returncode, 0)

    def test_state_reports_an_unsupported_profile(self):
        self.assertEqual(self.run_helper("state", usb_network="0").stdout, "enabled=1\nsupported=0\n")

    def test_set_off_saves_and_stops_the_link(self):
        self.assertEqual(self.run_helper("set", "off").returncode, 0)
        with open(self.config, encoding="utf-8") as handle:
            self.assertEqual(handle.read(), "enabled=0\n")
        self.assertEqual(
            self.systemctl_calls(), ["stop armada-usb-ssh.socket armada-usb-network.service"]
        )

    def test_set_on_saves_and_starts_the_link(self):
        self.write_switch("enabled=0\n")
        self.assertEqual(self.run_helper("set", "on").returncode, 0)
        with open(self.config, encoding="utf-8") as handle:
            self.assertEqual(handle.read(), "enabled=1\n")
        self.assertEqual(self.systemctl_calls(), ["start armada-usb-network.service"])

    def test_set_rebuilds_the_mtp_gadget_while_file_transfer_is_on(self):
        self.assertEqual(self.run_helper("set", "off", mtp_active=True).returncode, 0)
        self.assertEqual(
            self.systemctl_calls(),
            [
                "stop armada-usb-ssh.socket armada-usb-network.service",
                "restart armada-mtp.service",
            ],
        )
        os.remove(self.calls)
        self.assertEqual(self.run_helper("set", "on", mtp_active=True).returncode, 0)
        self.assertEqual(
            self.systemctl_calls(),
            ["restart armada-mtp.service", "--no-block start armada-usb-ssh.socket"],
        )

    def test_set_on_starts_nothing_where_the_profile_forbids_it(self):
        self.assertEqual(self.run_helper("set", "on", usb_network="0").returncode, 0)
        self.assertEqual(self.systemctl_calls(), [])

    def test_set_rejects_other_values(self):
        self.assertEqual(self.run_helper("set", "maybe").returncode, 2)
        self.assertFalse(os.path.exists(self.config))

    def test_enabled_follows_the_device_profile(self):
        self.assertEqual(self.run_helper("enabled", usb_network="1").returncode, 0)
        self.assertNotEqual(self.run_helper("enabled", usb_network="").returncode, 0)
        self.assertNotEqual(self.run_helper("enabled", usb_network="0").returncode, 0)

    def test_attach_and_detach_the_network_function(self):
        link = os.path.join(self.gadget, "configs", "c.1", "ncm.usb0")
        function = os.path.join(self.gadget, "functions", "ncm.usb0")

        self.assertEqual(self.run_helper("attach", self.gadget, usb_network="1").returncode, 0)
        self.assertTrue(os.path.isdir(function))
        self.assertEqual(os.readlink(link), function)

        # Attaching twice is harmless.
        self.assertEqual(self.run_helper("attach", self.gadget, usb_network="1").returncode, 0)

        self.assertEqual(self.run_helper("detach", self.gadget, usb_network="1").returncode, 0)
        self.assertFalse(os.path.lexists(link))
        self.assertFalse(os.path.exists(function))

    def test_attach_does_nothing_without_the_profile_flag(self):
        self.assertEqual(self.run_helper("attach", self.gadget, usb_network="").returncode, 0)
        self.assertEqual(os.listdir(os.path.join(self.gadget, "functions")), [])
        self.assertEqual(os.listdir(os.path.join(self.gadget, "configs", "c.1")), [])

    def test_attach_does_nothing_with_the_switch_off(self):
        self.write_switch("enabled=0\n")
        self.assertEqual(self.run_helper("attach", self.gadget).returncode, 0)
        self.assertEqual(os.listdir(os.path.join(self.gadget, "functions")), [])

    def test_unknown_command(self):
        self.assertEqual(self.run_helper("bogus").returncode, 2)


class UnitTest(unittest.TestCase):
    def test_link_runs_only_where_enabled_and_not_beside_mtp(self):
        service = raw("armada-usb-network.service")
        self.assertIn("ExecCondition=/usr/libexec/armada/usb-network enabled", service)
        self.assertIn("! systemctl is-active --quiet armada-mtp.service", service)
        self.assertIn("ExecStop=/usr/libexec/armada/usb-network stop", service)

    def test_ssh_socket_is_usb_only_and_steps_aside_for_sshd(self):
        socket = unit("armada-usb-ssh.socket")
        self.assertEqual(socket["Unit"]["Conflicts"], "sshd.service")
        self.assertEqual(socket["Socket"]["ListenStream"], "172.16.42.1:22")
        self.assertEqual(socket["Socket"]["FreeBind"], "yes")
        self.assertEqual(socket["Socket"]["Accept"], "yes")
        self.assertEqual(socket["Socket"]["IPAddressAllow"], "172.16.42.0/24")
        self.assertEqual(socket["Socket"]["IPAddressDeny"], "any")
        # No [Install]: only the link (or sshd stopping) starts it.
        self.assertNotIn("Install", socket)

    def test_ssh_session_is_key_only(self):
        session = unit("armada-usb-ssh@.service")["Service"]["ExecStart"]
        self.assertIn("sshd -i", session)
        self.assertIn("PasswordAuthentication=no", session)
        self.assertIn("KbdInteractiveAuthentication=no", session)
        self.assertIn("PermitRootLogin=no", session)

    def test_mtp_drop_in_hooks_are_in_the_right_lists(self):
        text = raw("armada-mtp.service.d", "50-usb-network.conf")
        lines = [line for line in text.splitlines() if line and not line.startswith("#")]
        start_pre = [line for line in lines if line.startswith("ExecStartPre=")]
        self.assertEqual(len(start_pre), 2)
        self.assertIn("systemctl stop armada-usb-network.service", start_pre[0])
        self.assertIn("usb-network attach", start_pre[1])
        self.assertTrue(any(l.startswith("ExecStop=") and "detach" in l for l in lines))
        self.assertTrue(any(l.startswith("ExecStopPost=") and "resume" in l for l in lines))

    def test_sshd_drop_in_restores_the_socket(self):
        text = raw("sshd.service.d", "50-armada-usb-ssh.conf")
        self.assertIn("ExecStopPost=-/usr/libexec/armada/usb-network ssh-resume", text)

    def test_keyfile_matches_only_gadget_interfaces(self):
        parser = configparser.ConfigParser(strict=False, interpolation=None)
        parser.read(NM_KEYFILE, encoding="utf-8")
        self.assertEqual(parser["connection"]["interface-name"], "usb0")
        self.assertEqual(
            parser["match"]["driver"],
            "configfs-gadget.armada-usb-network;configfs-gadget.armada-mtp;",
        )
        self.assertEqual(parser["ipv4"]["method"], "shared")
        self.assertEqual(parser["ipv4"]["address1"], "172.16.42.1/24")


    def test_udev_rule_hands_only_armada_gadgets_to_networkmanager(self):
        rule = os.path.join(ROOT, "system_files", "usr", "lib", "udev", "rules.d", "86-armada-usb-network.rules")
        with open(rule, encoding="utf-8") as handle:
            lines = [l for l in handle.read().splitlines() if l and not l.startswith("#")]
        self.assertEqual(len(lines), 1)
        self.assertIn('ENV{DEVTYPE}=="gadget"', lines[0])
        self.assertIn('DRIVERS=="configfs-gadget.armada-usb-network|configfs-gadget.armada-mtp"', lines[0])
        self.assertIn('ENV{NM_UNMANAGED}="0"', lines[0])


class ProfileTest(unittest.TestCase):
    def test_the_link_is_on_by_default_and_no_profile_turns_it_off(self):
        with open(os.path.join(DEVICES, "defaults.conf"), encoding="utf-8") as handle:
            self.assertIn("\nARMADA_USB_NETWORK=1\n", handle.read())
        for name in sorted(os.listdir(DEVICES)):
            if not name.endswith(".conf") or name == "defaults.conf":
                continue
            with open(os.path.join(DEVICES, name), encoding="utf-8") as handle:
                self.assertNotIn("ARMADA_USB_NETWORK=", handle.read(), name)


class ControlTest(unittest.TestCase):
    def test_freedom_control_can_read_and_set_the_switch(self):
        with open(CONTROL, encoding="utf-8") as handle:
            text = handle.read()
        self.assertIn('"get_usb_network_enabled": action_get_usb_network_enabled', text)
        self.assertIn('"set_usb_network_enabled": action_set_usb_network_enabled', text)
        self.assertIn('[USB_NETWORK, "set", "on" if enabled else "off"]', text)


if __name__ == "__main__":
    unittest.main()
