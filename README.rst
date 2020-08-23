=======================================
Apple Cinema Display Brightness Control
=======================================

Compiling
---------

Clone this repository::

    git clone https://github.com/warvariuc/acdcontrol.git

Change to program directory and compile it::

    cd acdcontrol
    make

A new file ``acdcontrol`` should appear in the same directory. If compiling failed, check if you
have installed packages necessary for compiling (e.g. ``build-essential``).

or

execute the ./acd-compile.sh script, that will execute the docker-compose.yml file. (requires that docker, docker-compose is installed)

Usage
-----

::

  ./acdcontrol [--silent|-s] [--brief|-b] [--help|-h] [--about|-a] [--detect|-d] [--list-all|-l] <hid device(s)> [<brightness>]


NOTE: You must have write permissions to this device in order to control the display being a
user, not root. If you have no such permissions, you can either grant read/write permission to
the world::

    sudo chmod a+rw /dev/usb/hiddevX

or change the ownership::

    sudo chown <your user name>:users /dev/usb/hiddevX


NOTE: It should be safe to run the program on other device than Apple Cinema/Studio display as
the program checks whether the device is Apple display and warns about it.


Parameters
----------

\-s, --silent
    Suppress non-functional program output

\-b, --brief
    Print brightness value only when in query mode, otherwise ignored.

\-h, --help
    Show short help message and quit.

\-a, --about
    Show information about the program, some credits and thanks.

\-d, --detect
    Run program in the detection mode. In this mode no writes are performed to device so you can
    use any number of files if you are not sure that your monitor(s) are supported.

\-l, --list-all
    Lists all "officially" supported monitors and quits. If you do not see the device, this does
    not mean that it won't work it simply means that I did not tested it on such a device. I'll
    appreciate if you submit your information in the forum for any display that works hid device
    device that represents your Apple Cinema display. It should be one of ``/dev/usb/hiddevX`` or
    ``/dev/hiddevX``.

brightness
    When this option is specified, the operation is to set brightness, otherwise, the current
    brightness is retrieved. If brightness starts with ``+`` or ``-``, the current brightness is
    increased or decreased by that value. (Note: You have to place -- before the negative value!)

    Brightness is a parameter ranged ``[0-255]``.
    Note, that not every value toggles the backlight power; different Apple Display models have
    different granularity. I use Apple Cinema 20" (clear plastic) and I'm feeling comfortable with
    the value of 160. I set 0, however, to see films in the darkness.

    See also: ``--brief`` option and "Known Limitations" section.


Usage examples
--------------

In the following examples I assume that your HID device is ``/dev/hiddev0``. You may have it as
``/dev/hiddevX`` or ``/dev/usb/hiddevX``.

acdcontrol --help
    Show long help message.

acdcontrol --detect /dev/hiddev*
    Perform detection, which HID device is actually your display to be controlled.

acdcontrol /dev/hiddev0
    Read current brightness parameter

acdcontrol /dev/hiddev0 160
    Set brightness to 160. Note, that brightness setting depends on your model. Generally, this
    parameter may get values in the range ``[0-255]``.

acdcontrol /dev/hiddev0 +10
    Increment current brightness by 10.

acdcontrol /dev/hiddev0 -- -10
    Decrement current brightness by 10. Please,note ``--``!


Sample Profiles
----------

00profile-low.sh
    Set brightness over usb to 0

01profile-middle.sh
    Set brightness over usb to 127.5

02profile-high.sh
    Set brightness over usb to 255

Known Limitations
-----------------

Currently, the display detection process is not fully automated as you need to specify a HID
device path.

First time (after boot up) brightness is retrieved *incorrectly* (zero value), however, after it is
set, the return value is correct. It shouldn't concern you much as Cinema/Studio Display stores
actual brightness settings between the sessions.
