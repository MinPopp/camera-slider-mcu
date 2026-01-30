#!/usr/bin/env python3
"""
MCU Protocol Tests

Tests the UART command protocol for the camera slider MCU.
Run with: pytest test_mcu_protocol.py -v --port /dev/ttyUSB0
"""

import pytest
import serial
import time
import re


@pytest.fixture(scope="session")
def serial_port(request):
    port = request.config.getoption("--port")
    baud = request.config.getoption("--baud")

    ser = serial.Serial(port, baud, timeout=2)
    time.sleep(0.1)
    ser.reset_input_buffer()
    yield ser
    ser.close()


def send_command(ser: serial.Serial, cmd: str, timeout: float = 2.0) -> str:
    """Send a command and return the response line."""
    ser.reset_input_buffer()
    ser.write(f"{cmd}\n".encode())
    ser.flush()

    start = time.time()
    response = b""
    while time.time() - start < timeout:
        if ser.in_waiting:
            response += ser.read(ser.in_waiting)
            if b"\n" in response:
                break
        time.sleep(0.01)

    return response.decode().strip()


class TestPing:
    def test_ping_returns_ok(self, serial_port):
        response = send_command(serial_port, "PING")
        assert response == "OK"

    def test_ping_with_extra_spaces(self, serial_port):
        response = send_command(serial_port, "  PING  ")
        assert response == "OK"


class TestStatus:
    def test_status_returns_valid_format(self, serial_port):
        response = send_command(serial_port, "STATUS")
        assert response.startswith("OK STATE=")
        assert "POS=" in response
        assert "HOMED=" in response

    def test_status_state_is_valid(self, serial_port):
        response = send_command(serial_port, "STATUS")
        match = re.search(r"STATE=(\w+)", response)
        assert match is not None
        state = match.group(1)
        assert state in ("idle", "moving", "homing", "error")

    def test_status_position_is_integer(self, serial_port):
        response = send_command(serial_port, "STATUS")
        match = re.search(r"POS=(-?\d+)", response)
        assert match is not None

    def test_status_homed_is_boolean(self, serial_port):
        response = send_command(serial_port, "STATUS")
        match = re.search(r"HOMED=([01])", response)
        assert match is not None


class TestGetPos:
    def test_getpos_returns_position(self, serial_port):
        response = send_command(serial_port, "GETPOS")
        assert response.startswith("OK POS=")
        match = re.search(r"POS=(-?\d+)", response)
        assert match is not None


class TestMove:
    def test_move_with_steps_and_speed(self, serial_port):
        response = send_command(serial_port, "MOVE STEPS=100 SPEED=500")
        assert response == "OK" or response.startswith("ERROR")

    def test_move_with_only_steps(self, serial_port):
        response = send_command(serial_port, "MOVE STEPS=100")
        assert response == "OK" or response.startswith("ERROR")

    def test_move_negative_steps(self, serial_port):
        response = send_command(serial_port, "MOVE STEPS=-100 SPEED=500")
        assert response == "OK" or response.startswith("ERROR")

    def test_move_missing_steps_returns_error(self, serial_port):
        response = send_command(serial_port, "MOVE SPEED=500")
        assert response.startswith("ERROR")
        assert "MISSING_STEPS" in response

    def test_move_invalid_speed_returns_error(self, serial_port):
        response = send_command(serial_port, "MOVE STEPS=100 SPEED=0")
        assert response.startswith("ERROR")


class TestStop:
    def test_stop_returns_position(self, serial_port):
        response = send_command(serial_port, "STOP")
        assert response.startswith("OK POS=")
        match = re.search(r"POS=(-?\d+)", response)
        assert match is not None


class TestHome:
    def test_home_returns_ok_or_busy(self, serial_port):
        response = send_command(serial_port, "HOME")
        assert response == "OK" or response.startswith("ERROR")


class TestUnknownCommand:
    def test_unknown_command_returns_error(self, serial_port):
        response = send_command(serial_port, "FOOBAR")
        assert response.startswith("ERROR 30")
        assert "UNKNOWN_COMMAND" in response

    def test_empty_command_no_response(self, serial_port):
        # Empty line should not produce a response
        serial_port.reset_input_buffer()
        serial_port.write(b"\n")
        serial_port.flush()
        time.sleep(0.1)
        # Verify we can still ping
        response = send_command(serial_port, "PING")
        assert response == "OK"


class TestSequence:
    """Test command sequences to verify state transitions."""

    def test_status_after_stop(self, serial_port):
        send_command(serial_port, "STOP")
        response = send_command(serial_port, "STATUS")
        match = re.search(r"STATE=(\w+)", response)
        assert match is not None
        assert match.group(1) == "idle"

    def test_getpos_matches_status(self, serial_port):
        status = send_command(serial_port, "STATUS")
        getpos = send_command(serial_port, "GETPOS")

        status_pos = re.search(r"POS=(-?\d+)", status)
        getpos_pos = re.search(r"POS=(-?\d+)", getpos)

        assert status_pos is not None
        assert getpos_pos is not None
        assert status_pos.group(1) == getpos_pos.group(1)


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
