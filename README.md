# EDAC Monitor

This utility offers the possibility to track EDAC failures and trigger
necessary actions. It is platform independent and finds all
main counter devices for correctable and uncorrectable errors
registered by the kernel in the sysfs. Refer to sysfs structure:

/sys/devices/system/edac/[memory-controller]/[controller-num]/[ue_count|ce_count]

Further reference about the sysfs structure can be found here:
https://www.kernel.org/doc/Documentation/driver-api/edac.rst

Example:
This calls EDAC monitor to run in background mode and track changes in
the EDAC counter devices:

	edac-mon --ce_count_store=/mem/path --ue_count_store=/mem/path -R

When --ue_count_store and/or --ce_count_store is given, occurrences will be summed
in addition to some optional memory location. This is for statistic how often
ECC error happens and a decision helper in terms of exchanging the particular HW part.

The -R option will trigger a reboot system call after UE occurence. This makes
sense as system is in an unreliable state when such error happens.

In addition, the EDAC subsystem in the kernel doesn't offer yet sysfs notify, or blocking poll
mode. So, polling in monitor mode has to be done on user-space level.
EDAC monitor offers the options --poll_timeout_ce and --poll_timeout_ue where timeout
in seconds can be specified how often EDAC counter devices should be checked for
changes. Example:

	edac-mon --ce_count_store=/mem/path --ue_count_store=/mem/path --poll_timeout_ce=200 --poll_timeout_ue=5 -R

Default poll timeout for UE is 3 sec and CE is 120 sec.

Further options can be displayed via:

	edac-mon -h

# Install
This is a C program including a Makefile - no buildsystem arround:

Simple compilation on target:

	make

Cross compilation for example in case of ARM64:

	make CC=aarch64-linux-gnu-gcc

Optional: pass CFLAGS to make

	make CC=aarch64-linux-gnu-gcc CFLAGS=-g

Installing:

	make install INSTALL_PREFIX_BIN=/path/to/install

Default install path is /usr/bin when INSTALL_PREFIX_BIN is not given as parameter.

There is in addition a systemd template for this unit.
But this might variate to the system requirements.

Default install path for systemd units is typically:

	/usr/lib/systemd/system/

To enable this unit:

	systemctl enable edac-mon.service

This service depends on loaded EDAC drivers by the kernel. If edac-mon will not
find any ce/ue_count device, service will fail to start.
