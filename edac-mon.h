/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (C) 2019, Wladislav Wiebe, <wladislav.kw@gmail.com>
 *
 */

#define VERSION "1.1.1"

#define handle_error(msg) \
	do { fprintf(stderr, "%s:%d: %s\n", __FUNCTION__, __LINE__, msg); \
	exit(EXIT_FAILURE); } while (0)

#define handle_error_en(en, msg) \
	do { errno = en; fprintf(stderr, "%s:%d: %s : %s\n", __FUNCTION__, __LINE__, msg, strerror(en)); \
	exit(EXIT_FAILURE); } while (0)

#define handle_error_en_opt(en, msg, msg_opt) \
	do { errno = en; fprintf(stderr, "%s:%d: %s %s: %s\n", __FUNCTION__, __LINE__, msg, msg_opt, strerror(en)); \
	exit(EXIT_FAILURE); } while (0)

const char *edac_sysfs_path = "/sys/devices/system/edac/";
char *add_sysfs_base_path = NULL;
long add_sysfs_search_depth = 3;
const long ce_poll_timeout_default = 120;
const long ue_poll_timeout_default = 3;
unsigned int store_as_uint32 = 0;

struct edac_counter_device {
	char **edac_count_list;
	char *edac_count_ext_mem_path;
	char edac_count_type[9];
	uint32_t dev_count;
	unsigned int oneshot;
	unsigned int reboot_on_ue;
	unsigned int reset_ext_mem;
	long dev_poll_timeout;
};
