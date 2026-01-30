"""Pytest configuration for MCU protocol tests."""

import pytest


def pytest_addoption(parser):
    parser.addoption(
        "--port",
        action="store",
        default="/dev/ttyUSB0",
        help="Serial port for MCU connection",
    )
    parser.addoption(
        "--baud",
        action="store",
        default=115200,
        type=int,
        help="Baud rate (default: 115200)",
    )
