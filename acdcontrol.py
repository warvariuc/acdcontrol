#! /usr/bin/env python3
"""This program allows changing brightness on some Apple displays.
"""
__author__ = 'Victor Varvariuc <victor.varvariuc@gmail.com>'

import argparse
import struct
import ctypes
import fcntl
import os
import sys
from collections import OrderedDict

import ioctl


SUPPORTED_DEVICES = {
    (0x05ac, 'Apple'): (
        (0x9215, 'Apple Studio Display 15"'),
        (0x9217, 'Apple Studio Display 17"'),
        (0x9218, 'Apple Cinema Display 23"'),
        (0x9219, 'Apple Cinema Display 20"'),
        (0x921e, 'Apple Cinema Display 24"'),
        (0x9221, 'Apple Cinema Display 30"'),
        (0x9226, 'Apple Cinema HD Display 27"'),
        (0x9227, 'Apple Cinema HD Display 27" 2013'),
        (0x9232, 'Apple Cinema HD Display 30"'),
        (0x9236, 'Apple LED Cinema Display 24"'),
    ),
    (0x0419, 'Samsung Electronics'): (
        (0x8002, 'Samsung SyncMaster 757NF'),
    ),
}


# -------------------------------------------------------------------------------------------------
class StructMeta(type(ctypes.Structure)):
    @classmethod
    def __prepare__(metacls, cls_name, cls_bases):
        """Use this ordered dict as class dictionary during class definition.
        """
        return OrderedDict()

    def __new__(metacls, cls_name, cls_bases, cls_dict):
        fields = []
        for field_name, field in list(cls_dict.items()):
            if isinstance(field, type) and issubclass(field, ctypes._SimpleCData):
                fields.append((field_name, field))
                del cls_dict[field_name]
        cls_dict['_fields_'] = fields
        result = super(StructMeta, metacls).__new__(
            metacls, cls_name, cls_bases, dict(cls_dict))
        return result

    def __len__(self):
        """Byte length."""
        _format = ''.join(field[1]._type_ for field in self._fields_)
        return struct.calcsize(_format)


class Struct(ctypes.Structure, metaclass=StructMeta):
    def __repr__(self):
        values = []
        for field_name, _ in self._fields_:
            value = getattr(self, field_name)
            value = hex(value) if isinstance(value, int) else repr(value)
            values.append('%s=%s' % (field_name, value))
        return '%s(%s)' % (self.__class__.__name__, ', '.join(values))


# /usr/src/linux-headers-3.11.0-14/include/uapi/linux/hiddev.h
# -------------------------------------------------------------------------------------------------
# Structures

class hid_version(Struct):
    v3 = ctypes.c_ubyte
    v2 = ctypes.c_ubyte
    v1 = ctypes.c_ushort


class hiddev_devinfo(Struct):
    bustype = ctypes.c_uint
    busnum = ctypes.c_uint
    devnum = ctypes.c_uint
    ifnum = ctypes.c_uint
    vendor = ctypes.c_ushort
    product = ctypes.c_ushort
    version = ctypes.c_ushort
    num_applications = ctypes.c_uint


class hiddev_usage_ref(Struct):
    report_type = ctypes.c_uint
    report_id = ctypes.c_uint
    field_index = ctypes.c_uint
    usage_index = ctypes.c_uint
    usage_code = ctypes.c_uint
    value = ctypes.c_int


class hiddev_report_info(Struct):
    report_type = ctypes.c_uint
    report_id = ctypes.c_uint
    num_fields = ctypes.c_uint


HID_REPORT_ID_UNKNOWN = 0xffffffff
HID_REPORT_ID_FIRST = 0x00000100
HID_REPORT_ID_NEXT = 0x00000200
HID_REPORT_ID_MASK = 0x000000ff
HID_REPORT_ID_MAX = 0x000000ff

HID_REPORT_TYPE_INPUT = 1
HID_REPORT_TYPE_OUTPUT = 2
HID_REPORT_TYPE_FEATURE = 3
HID_REPORT_TYPE_MIN = 1
HID_REPORT_TYPE_MAX = 3

# -------------------------------------------------------------------------------------------------
# IOCTLs (0x00 - 0x7f)

HIDIOCGVERSION = ioctl.IOR(ord('H'), 0x01, len(hid_version))
HIDIOCAPPLICATION = ioctl.IO(ord('H'), 0x02)
HIDIOCGDEVINFO = ioctl.IOR(ord('H'), 0x03, len(hiddev_devinfo))
# HIDIOCGSTRING = ioctl.IOR(ord('H'), 0x04, len(hiddev_string_descriptor))
HIDIOCINITREPORT = ioctl.IO(ord('H'), 0x05)
# HIDIOCGNAME(len)	_IOC(_IOC_READ, 'H', 0x06, len)
HIDIOCGREPORT = ioctl.IOW(ord('H'), 0x07, len(hiddev_report_info))
HIDIOCSREPORT = ioctl.IOW(ord('H'), 0x08, len(hiddev_report_info))
HIDIOCGREPORTINFO = ioctl.IOWR(ord('H'), 0x09, len(hiddev_report_info))
# HIDIOCGFIELDINFO	_IOWR('H', 0x0A, struct hiddev_field_info)
HIDIOCGUSAGE = ioctl.IOWR(ord('H'), 0x0B, len(hiddev_usage_ref))  # get
HIDIOCSUSAGE = ioctl.IOW(ord('H'), 0x0C, len(hiddev_usage_ref))  # set
HIDIOCGUCODE = ioctl.IOWR(ord('H'), 0x0D, len(hiddev_usage_ref))
HIDIOCGFLAG = ioctl.IOR(ord('H'), 0x0E, struct.calcsize(ctypes.c_ushort._type_))
# HIDIOCSFLAG		_IOW('H', 0x0F, int)
HIDIOCGCOLLECTIONINDEX = ioctl.IOW(ord('H'), 0x10, len(hiddev_usage_ref))
# HIDIOCGCOLLECTIONINFO	_IOWR('H', 0x11, struct hiddev_collection_info)
# HIDIOCGPHYS(len)	_IOC(_IOC_READ, 'H', 0x12, len)

# -------------------------------------------------------------------------------------------------
BRIGHTNESS_CONTROL = 16
USAGE_CODE = 0x820010
# -------------------------------------------------------------------------------------------------


arg_parser = argparse.ArgumentParser(
    description='Set brightness on Apple and some other USB monitors.')
arg_parser.add_argument('device', nargs='?', help='Path to the HID device')
arg_parser.add_argument(
    'brightness', nargs='?', default='',
    help='New brightness level. If starts with +/-, it will be increased/decreased.')
args = arg_parser.parse_args()

if not args.device:
    arg_parser.print_help()
    sys.exit(1)


mode = os.O_RDWR
device_path = args.device

device_handle = os.open(device_path, mode)

version = hid_version()

fcntl.ioctl(device_handle, HIDIOCGVERSION, version)
print('hiddev driver version is %d.%d.%d' % (version.v1, version.v2, version.v3))

# suck out some device information
device_info = hiddev_devinfo()
fcntl.ioctl(device_handle, HIDIOCGDEVINFO, device_info)
for (vendor_id, vendor_name), products in SUPPORTED_DEVICES.items():
    if device_info.vendor == vendor_id:
        break
else:
    print('Vendor %04x is not supported.' % device_info.vendor)
    raise SystemExit

for product_id, product_name in products:
    if device_info.product == product_id:
        break
else:
    print('Product %04x is not supported.' % device_info.product)
    raise SystemExit

print('Found supported product %04x (%s) of vendor %04x (%s)' % (
    device_info.product, product_name, device_info.vendor, vendor_name))


# Now that we have the number of applications, we can retrieve them
# using the HIDIOCAPPLICATION ioctl() call
# applications are indexed from 0..{num_applications-1}
for app_num in range(device_info.num_applications):
    application = fcntl.ioctl(device_handle, HIDIOCAPPLICATION, app_num)
    # The magic values come from various usage table specs
    if (application >> 16) & 0xFF == 0x80:
        break
else:
    print('The device is NOT a USB monitor!')
    raise SystemExit


# Initialise the internal report structures
if fcntl.ioctl(device_handle, HIDIOCINITREPORT, 0) < 0:
    raise SystemExit("FATAL: Failed to initialize internal report structures")

usage_ref = hiddev_usage_ref(
    report_type=HID_REPORT_TYPE_FEATURE, report_id=BRIGHTNESS_CONTROL, field_index=0,
    usage_index=0, usage_code=USAGE_CODE)

rep_info = hiddev_report_info(
    report_type=HID_REPORT_TYPE_FEATURE, report_id=BRIGHTNESS_CONTROL, num_fields=1)

# get current brightness
if fcntl.ioctl(device_handle, HIDIOCGUSAGE, usage_ref) < 0:
    raise SystemExit("Usage failed!")
if fcntl.ioctl(device_handle, HIDIOCGREPORT, rep_info) < 0:
    raise SystemExit("Report failed!")

print('Current brightness is: %d' % usage_ref.value)

if not args.brightness:
    sys.exit(0)

brightness = int(args.brightness)

if args.brightness.startswith('+') or args.brightness.startswith('-'):
    # increase/decrease brightness
    brightness += usage_ref.value

# set brightness
usage_ref.value = brightness = max(0, brightness)
if fcntl.ioctl(device_handle, HIDIOCSUSAGE, usage_ref) < 0:
    raise SystemExit("Usage failed!")
if fcntl.ioctl(device_handle, HIDIOCSREPORT, rep_info) < 0:
    raise SystemExit("Report failed!")
