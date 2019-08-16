/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (C) 2019, Wladislav Wiebe, <wladislav.kw@gmail.com>
 *
 */

#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <ftw.h>
#include <limits.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>
#include <linux/reboot.h>
#include <sys/reboot.h>
#include <sys/syscall.h>
#include "edac-mon.h"

void get_edac_count_devices(struct edac_counter_device *ecd) {
	long search_depth = 3;
	ecd->dev_count = 0;
	ecd->edac_count_list = malloc(sizeof(char *));

	if (ecd->edac_count_list == NULL)
		handle_error("Failed to malloc edac_count_list");

	int search_edac_count_devices(const char *fpath, const struct stat *sb,
				      int tflag, struct FTW *ftwbuf) {
		if (ftwbuf->level != search_depth)
			return 0;

		if (tflag == FTW_F) {
			if (strstr(fpath, ecd->edac_count_type) != NULL) {
				ecd->edac_count_list = realloc(ecd->edac_count_list,
						       sizeof(char *) * (ecd->dev_count + 1));
				if (ecd->edac_count_list == NULL)
					handle_error("Failed to malloc edac_count_list");

				ecd->edac_count_list[ecd->dev_count] = malloc(strlen(fpath) + 1);
				if (ecd->edac_count_list[ecd->dev_count] == NULL)
					handle_error("Failed to malloc edac_count_list");

				strcpy(ecd->edac_count_list[ecd->dev_count], fpath);
				ecd->dev_count++;
			}
		}
		return 0;
	}

	nftw(edac_sysfs_path, search_edac_count_devices, 20, FTW_PHYS|FTW_DEPTH);

	if (add_sysfs_base_path != NULL) {
		search_depth = add_sysfs_search_depth;
		nftw(add_sysfs_base_path, search_edac_count_devices, 20, FTW_PHYS|FTW_DEPTH);
	}

	if (ecd->dev_count == 0)
		handle_error_en(ENXIO, "No EDAC count devices found!");
}

uint32_t get_edac_value(char *path) {
	uint32_t edac_value;
	FILE *fp;

	fp = fopen(path, "r");
	if (fp == NULL)
		handle_error_en_opt(errno, "Failed to open: ", path);

	fscanf(fp, "%u", &edac_value);
	fclose(fp);

	return edac_value;
}

uint32_t get_edac_value_uint32(struct edac_counter_device *ecd) {
	uint32_t edac_value;
	int file, ret;

	file = open(ecd->edac_count_ext_mem_path, O_RDONLY);
	if (file < 0)
		handle_error_en_opt(errno, "Failed to open: ", ecd->edac_count_ext_mem_path);

	ret = read(file, (char *)&edac_value, 4);
	if (ret < 0)
		handle_error_en_opt(errno, "Failed to read: ", ecd->edac_count_ext_mem_path);

	if (ret != 4) {
		fprintf(stderr, "Broken uint32 content in %s\n"
				"Read %u instead of 4 bytes!\n",
				ecd->edac_count_ext_mem_path, ret);
		ecd->reset_ext_mem = 1;
	}

	close(file);

	return edac_value;
}

void write_edac_value_uint32(struct edac_counter_device *ecd,
			    uint32_t edac_value, uint32_t repair_value) {
	int file, ret;

	file = open(ecd->edac_count_ext_mem_path, O_RDWR);
	if (file < 0)
		handle_error_en_opt(errno, "Failed to open: ", ecd->edac_count_ext_mem_path);

	if (ecd->reset_ext_mem == 1) {
		fprintf(stderr, "Try to reset %s\n"
				"Writing current edac count val %u\n",
				ecd->edac_count_ext_mem_path, repair_value);
		ret = write(file, (char *)&repair_value, 4);
		if (ret < 0)
			handle_error_en_opt(errno, "Failed to write: ", ecd->edac_count_ext_mem_path);

		ecd->reset_ext_mem = 0;
		close(file);
		return;
	}

	ret = write(file, (char *)&edac_value, 4);
	if (ret < 0)
		handle_error_en_opt(errno, "Failed to write: ", ecd->edac_count_ext_mem_path);

	close(file);
}

void dump_edac_count_list(struct edac_counter_device *ecd) {
	int i;

	for (i = 0; i < ecd->dev_count; i++) {
		printf("%s : %s = %u\n", ecd->edac_count_type, ecd->edac_count_list[i],
					 get_edac_value(ecd->edac_count_list[i]));
	}
}

void dump_edac_count_ext_mem(struct edac_counter_device *ecd) {
	uint32_t edac_value;

	if (ecd->edac_count_ext_mem_path == NULL)
		return;

	if (store_as_uint32 == 0) {
		edac_value = get_edac_value(ecd->edac_count_ext_mem_path);
	} else {
		edac_value = get_edac_value_uint32(ecd);
		if (ecd->reset_ext_mem == 1)
			write_edac_value_uint32(ecd, 0, 0);
		edac_value = get_edac_value_uint32(ecd);
	}
	printf("EDAC: ext count: %s = %u\n", ecd->edac_count_ext_mem_path, edac_value);
}

void update_edac_count_ext_mem(struct edac_counter_device *ecd,
			       uint32_t edac_count_num, uint32_t repair_value) {
	uint32_t edac_ext_mem_count;
	uint32_t edac_ext_mem_count_upd;
	FILE *fp;

	if (store_as_uint32 == 0) {
		edac_ext_mem_count = get_edac_value(ecd->edac_count_ext_mem_path);
		edac_ext_mem_count_upd = edac_ext_mem_count + edac_count_num;
		fp = fopen(ecd->edac_count_ext_mem_path, "w");
		if (fp == NULL)
			handle_error_en_opt(errno, "Failed to open: ", ecd->edac_count_ext_mem_path);

		fprintf(fp, "%u", edac_ext_mem_count_upd);
		fclose(fp);
	} else {
		edac_ext_mem_count = get_edac_value_uint32(ecd);
		edac_ext_mem_count_upd = edac_ext_mem_count + edac_count_num;
		write_edac_value_uint32(ecd, edac_ext_mem_count_upd,
					repair_value);
	}
	printf("EDAC: updated external memory reference counter %s from %u to %u\n",
		ecd->edac_count_ext_mem_path, edac_ext_mem_count,
		edac_ext_mem_count_upd);
}

int inform_kmsg(void) {
	FILE *fp;

	fp = fopen("/dev/kmsg", "w");
	if (fp == NULL) {
		fprintf(stderr, "Failed to inform kernel about coming reboot syscall! Cannot open /dev/kmsg\n");
		return -1;
	}

	fprintf(fp, "%s", "<0>EDAC: ALERT: [user-space process 'edac-mon'] : called reboot due to UE occurrence!\n");
	fclose(fp);

	return 0;
}

void handle_edac_failure(struct edac_counter_device *ecd,
			 uint32_t edac_count_num, uint32_t dev_num) {
	int err;
	uint32_t repair_value = get_edac_value(ecd->edac_count_list[dev_num]);

	fprintf(stderr, "EDAC: occurence in %s : %s = %u\n",
		ecd->edac_count_type, ecd->edac_count_list[dev_num],
		repair_value);

	if (ecd->edac_count_ext_mem_path != NULL)
		update_edac_count_ext_mem(ecd, edac_count_num, repair_value);

	if (ecd->reboot_on_ue == 1) {
		fprintf(stderr, "EDAC: ALERT: try to reboot the system due to Uncorrectable Error(s)!\n");
		sync();
		inform_kmsg();
		err = reboot(LINUX_REBOOT_CMD_RESTART);
		/* We should not reach this code */
		fprintf(stderr, "EDAC: failed to reboot the system! %s\n", strerror(err));
		/* do not exit fail - let's monitor further UEs even if we fail to reboot */
	}
}

void check_edac_failures(struct edac_counter_device *ecd) {
	int i;
	uint32_t edac_count_ref[ecd->dev_count], edac_count_now;

	dump_edac_count_ext_mem(ecd);

	/* get values from first run to initialize reference value */
	for (i = 0; i < ecd->dev_count; i++) {
		edac_count_ref[i] = get_edac_value(ecd->edac_count_list[i]);
		if (!ecd->oneshot)
			printf("EDAC: monitoring: %s\n", ecd->edac_count_list[i]);
	}

	if (!ecd->oneshot)
		printf("EDAC: poll intervall for %s = %ld\n", ecd->edac_count_type,
						     ecd->dev_poll_timeout);
	else
		dump_edac_count_list(ecd);

	/* check if there are already EDAC occurences before this application started */
	for (i = 0; i < ecd->dev_count; i++) {
		if (edac_count_ref[i] > 0)
			handle_edac_failure(ecd, edac_count_ref[i], i);
	}

	while (1) {
		for (i = 0; i < ecd->dev_count; i++) {
			edac_count_now = get_edac_value(ecd->edac_count_list[i]);
			if (edac_count_now > edac_count_ref[i]) {
				handle_edac_failure(ecd, edac_count_now - edac_count_ref[i], i);
				edac_count_ref[i] = edac_count_now;
			}
		}

		if (ecd->oneshot)
			break;
		else
			sleep(ecd->dev_poll_timeout);
	}
}

void *check_edac_ce_failures(void *data) {
	struct edac_counter_device *ecd = (struct edac_counter_device *)data;
	check_edac_failures(ecd);

	return NULL;
}

void *check_edac_ue_failures(void *data) {
	struct edac_counter_device *ecd = (struct edac_counter_device *)data;
	check_edac_failures(ecd);

	return NULL;
}

void free_edac_devices(struct edac_counter_device *ecd) {
	int i;

	for (i = 0; i < ecd->dev_count; i++)
		free(ecd->edac_count_list[i]);
	free(ecd->edac_count_list);
}

void print_help(char *argv[]) {
	fprintf(stderr, "%s: missing or unknown operand\n", argv[0]);
	fprintf(stderr, "Try %s -h for more information.\n", argv[0]);
}

void print_usage(char *argv[]) {
	printf("Usage: %s [OPTIONS]\n\n", argv[0]);
	printf(" -h, --help                         Show this message\n");
	printf(" -v, --version                      Show version\n");
	printf(" -l, --list                         List all available EDAC devices\n");
	printf(" -o, --once                         Run only one time without polling EDAC devices, default is backgroung/polling mode\n");
	printf(" -R, --reboot                       Run in backgroung/polling mode and reboot the system on UE occurrence\n");
	printf("     --ce_count_store=[PATH]        Sum CE occurence to another memory location\n");
	printf("     --ue_count_store=[PATH]        Sum UE occurence to another memory location\n");
	printf("     --store_as_uint32              Store EDAC occurence as 32 bit bytestream - use case could be a reference counter in Device Tree\n");
	printf("     --poll_timeout_ce=[TIME]       Timeout in seconds to poll the EDAC CE counter devices for changes. Default is %ld sec\n", ce_poll_timeout_default);
	printf("     --poll_timeout_ue=[TIME]       Timeout in seconds to poll the EDAC UE counter devices for changes. Default is %ld sec\n", ue_poll_timeout_default);
	printf("     --add_sysfs_base_path=[PATH]   Add an additional base path for EDAC count devices. Default is %s\n", edac_sysfs_path);
	printf("     --add_sysfs_search_depth=[NUM] Change search depth for -add_sysfs_base_path, default is %s\n", edac_sysfs_path);
	printf("\nExample:\n");
	printf(" %s --ce_count_store=/mem/path --ue_count_store=/mem/path --poll_timeout_ce=120 --poll_timeout_ue=2 -R\n", argv[0]);
}

int main(int argc, char *argv[]) {
	int c, ret, list = 0;
	struct edac_counter_device ce_dev;
	struct edac_counter_device ue_dev;
	pthread_t t_check_edac_ce_failures, t_check_edac_ue_failures;
	const char *short_opt = "hloRv0:1:2:3:4:5:6:";

	struct option long_opt[] =
	{
		{"help",		   no_argument,		NULL, 'h'},
		{"list",		   no_argument,		NULL, 'l'},
		{"once",		   no_argument,		NULL, 'o'},
		{"reboot",		   no_argument,		NULL, 'R'},
		{"version",		   no_argument,		NULL, 'v'},
		{"ce_count_store",	   required_argument,	NULL, '0'},
		{"ue_count_store",	   required_argument,	NULL, '1'},
		{"store_as_uint32",	   no_argument,		NULL, '2'},
		{"poll_timeout_ce",	   required_argument,	NULL, '3'},
		{"poll_timeout_ue",	   required_argument,	NULL, '4'},
		{"add_sysfs_base_path",	   required_argument,	NULL, '5'},
		{"add_sysfs_search_depth", required_argument,	NULL, '6'},
		{NULL,			   0,			NULL, 0}
	};

	setbuf(stdout, NULL); /* Otherwise we might delay/lose logs in journal */

	/* Init EDAC count defaults */
	strcpy(ce_dev.edac_count_type, "ce_count");
	strcpy(ue_dev.edac_count_type, "ue_count");
	ce_dev.edac_count_ext_mem_path = NULL;
	ue_dev.edac_count_ext_mem_path = NULL;
	ce_dev.dev_poll_timeout = ce_poll_timeout_default;
	ue_dev.dev_poll_timeout = ue_poll_timeout_default;
	ce_dev.oneshot = 0;
	ue_dev.oneshot = 0;
	ce_dev.reboot_on_ue = 0;
	ue_dev.reboot_on_ue = 0;
	ce_dev.reset_ext_mem = 0;
	ue_dev.reset_ext_mem = 0;

	while((c = getopt_long(argc, argv, short_opt, long_opt, NULL)) != -1)
	{
		switch (c) {
			case 0:
				break;
			case 'h':
				print_usage(argv);
				return 0;
			case 'l':
				list = 1;
				break;
			case 'o':
				ce_dev.oneshot = 1;
				ue_dev.oneshot = 1;
				break;
			case 'R':
				ue_dev.reboot_on_ue = 1;
				break;
			case 'v':
				printf("%s Version %s\n", argv[0], VERSION);
				return 0;
			case '0':
				ce_dev.edac_count_ext_mem_path = optarg;
				break;
			case '1':
				ue_dev.edac_count_ext_mem_path = optarg;
				break;
			case '2':
				store_as_uint32 = 1;
				break;
			case '3':
				sscanf(optarg, "%ld", &ce_dev.dev_poll_timeout);
				break;
			case '4':
				sscanf(optarg, "%ld", &ue_dev.dev_poll_timeout);
				break;
			case '5':
				add_sysfs_base_path = optarg;
				break;
			case '6':
				sscanf(optarg, "%ld", &add_sysfs_search_depth);
				break;
			case ':':
			case '?':
				print_help(argv);
				return -1;
			default:
				print_help(argv);
				return -1;
		};
	};

	get_edac_count_devices(&ce_dev);
	get_edac_count_devices(&ue_dev);

	if (list == 1) {
		dump_edac_count_list(&ce_dev);
		dump_edac_count_list(&ue_dev);
		goto free_devices;
	}

	ret = pthread_create(&t_check_edac_ce_failures, NULL, &check_edac_ce_failures, (void *)&ce_dev);
	if (ret != 0)
		handle_error_en(ret, "pthread_create check_edac_ce_failures!");

	ret = pthread_create(&t_check_edac_ue_failures, NULL, &check_edac_ue_failures, (void *)&ue_dev);
	if (ret != 0)
		handle_error_en(ret, "pthread_create check_edac_ue_failures!");

	ret = pthread_join(t_check_edac_ce_failures, NULL);
	if (ret != 0)
		handle_error_en(ret, "pthread_join check_edac_ce_failures!");

	ret = pthread_join(t_check_edac_ue_failures, NULL);
	if (ret != 0)
		handle_error_en(ret, "pthread_join check_edac_ue_failures!");

free_devices:
	free_edac_devices(&ce_dev);
	free_edac_devices(&ue_dev);

	return 0;
}
