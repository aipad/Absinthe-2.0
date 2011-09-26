/**
 * GreenPois0n Absinthe - crashreport.c
 * Copyright (C) 2010 Chronic-Dev Team
 * Copyright (C) 2010 Joshua Hill
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 **/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <plist/plist.h>
#include <libimobiledevice/libimobiledevice.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "debug.h"
#include "crashreport.h"

crashreport_t* crashreport_create() {
	crashreport_t* report = (crashreport_t*) malloc(sizeof(crashreport_t));
	if (report) {
		memset(report, '\0', sizeof(crashreport_t));
	}
	return report;
}

void crashreport_free(crashreport_t* report) {
	if (report) {
		free(report);
	}
}

arm_state_t* crashreport_parse_state(const char* description) {
	arm_state_t* state = NULL;
	char* start;
	char line[256];	
	int num;
	char* lf;

	start = strstr(description, "ARM Thread State");
	if (!start) {
		error("Couldn't find ARM state beginning\n");
		return NULL;
	}

	start = strchr(start, '\n');
	if (!start) {
		error("Couldn't get linebreak after beginning line\n");
		return NULL;
	}
	start++;

	lf = strchr(start, '\n');
	if (!lf) {
		return NULL;
	}
	memcpy(line, start, lf-start);
	line[lf-start] = 0;

	state = (arm_state_t*)malloc(sizeof(arm_state_t));
	memset(state, 0, sizeof(arm_state_t));

	num = sscanf(line, "%*[ ]r0: 0x%08x%*[ ]r1: 0x%08x%*[ ]r2: 0x%08x%*[ ]r3: 0x%08x", &state->r0, &state->r1, &state->r2, &state->r3);
	if (num != 4) {
		free(state);
		return NULL;
	}

	start = lf+1;
	lf = strchr(start, '\n');
	if (!lf) {
		free(state);
		return NULL;
	}
	memcpy(line, start, lf-start);
	line[lf-start] = 0;

	num = sscanf(line, "%*[ ]r4: 0x%08x%*[ ]r5: 0x%08x%*[ ]r6: 0x%08x%*[ ]r7: 0x%08x", &state->r4, &state->r5, &state->r6, &state->r7);
	if (num != 4) {
		free(state);
		return NULL;
	}

	start = lf+1;
	lf = strchr(start, '\n');
	if (!lf) {
		free(state);
		return NULL;
	}
	memcpy(line, start, lf-start);
	line[lf-start] = 0;

	num = sscanf(line, "%*[ ]r8: 0x%08x%*[ ]r9: 0x%08x%*[ ]r10: 0x%08x%*[ ]r11: 0x%08x", &state->r8, &state->r9, &state->r10, &state->r11);
	if (num != 4) {
		free(state);
		return NULL;
	}

	start = lf+1;
	lf = strchr(start, '\n');
	if (!lf) {
		free(state);
		return NULL;
	}
	memcpy(line, start, lf-start);
	line[lf-start] = 0;

	num = sscanf(line, "%*[ ]ip: 0x%08x%*[ ]sp: 0x%08x%*[ ]lr: 0x%08x%*[ ]pc: 0x%08x", &state->ip, &state->sp, &state->lr, &state->pc);
	if (num != 4) {
		free(state);
		return NULL;
	}

	start = lf+1;
	lf = strchr(start, '\n');
	if (!lf) {
		free(state);
		return NULL;
	}
	memcpy(line, start, lf-start);
	line[lf-start] = 0;

	num = sscanf(line, "%*[ ]cpsr: 0x%08x", &state->cpsr);
	if (num != 1) {
		free(state);
		return NULL;
	}

	debug("ARM State = {\n\tr0:%08x r1:%08x  r2:%08x  r3:%08x\n"
	       "\tr4:%08x r5:%08x  r6:%08x  r7:%08x\n"
	       "\tr8:%08x r9:%08x r10:%08x r11:%08x\n"
	       "\tip:%08x sp:%08x  lr:%08x  pc:%08x\n"
	       "\tcpsr:%08x\n}\n",
	       state->r0, state->r1, state->r2, state->r3,
	       state->r4, state->r5, state->r6, state->r7,
	       state->r8, state->r9, state->r10, state->r11,
	       state->ip, state->sp, state->lr, state->pc,
	       state->cpsr);

	return state;
}

dylib_info_t** crashreport_parse_dylibs(const char* description) {
	dylib_info_t** dylibs = NULL;
	int dylibs_cnt = 256;
	int num_dylibs = 0;

	char* start;
	char line[256];	
	int num;
	char* lf;

	start = strstr(description, "Binary Images:");
	if (!start) {
		error("Couldn't find Binary Images beginning\n");
		return NULL;
	}

	start = strchr(start, '\n');
	if (!start) {
		error("Couldn't get linebreak after beginning line\n");
		return NULL;
	}

	dylibs = (dylib_info_t**)malloc(sizeof(dylib_info_t*) * dylibs_cnt);
	memset(dylibs, 0, sizeof(dylib_info_t*) * dylibs_cnt);

	start++;
	lf = strchr(start, '\n');
	while (lf) {
		memcpy(line, start, lf-start);
		line[lf-start] = 0;
		char *lineptr = line;
		uint32_t offset = 0;
		char imagename[256];
		imagename[0] = 0;
		while ((lineptr[0] != 0) && (lineptr[0] == ' ')) {
			lineptr++;
		}
		num = sscanf(lineptr, "0x%x -%*[ ]0x%*[0-9a-fA-F]%*[ ]%s arm", &offset, imagename);
		if (num == 2) {
			if (num_dylibs >= dylibs_cnt) {
				dylibs_cnt+=64;
				dylibs = (dylib_info_t**)realloc(dylibs, sizeof(dylib_info_t*) * dylibs_cnt);
				if (!dylibs) {
					error("ERROR: Out of memory\n");
					return NULL;
				}
			}
			dylibs[num_dylibs] = (dylib_info_t*)malloc(sizeof(dylib_info_t));
			if (!dylibs[num_dylibs]) {
				error("ERROR: Out of memory\n");
				return NULL;
			}
			dylibs[num_dylibs]->offset = offset;
			dylibs[num_dylibs]->name = strdup(imagename);
			num_dylibs++;
		}

		start = lf+1;
		lf = strchr(start, '\n');
	}

	// add a NULL terminator
	if (num_dylibs >= dylibs_cnt) {
		dylibs_cnt++;
		dylibs = (dylib_info_t**)realloc(dylibs, sizeof(dylib_info_t*) * dylibs_cnt);
		if (!dylibs) {
			error("ERROR: Out of memory\n");
			return NULL;
		}
	}
	dylibs[num_dylibs] = NULL;

#ifdef _DEBUG
	{
		debug("Number of binary images: %d {\n", num_dylibs);
		int i;
		while (dylibs[i]) {
			debug("\t%d:\t0x%08x: %s\n", i, dylibs[i]->offset, dylibs[i]->name);
			i++;
		}
		debug("}\n");
	}
#endif

	return dylibs;
}

thread_info_t** crashreport_parse_threads(const char* description) {
	return NULL;
}

crashreport_t* crashreport_parse_plist(plist_t plist) {
	char* description = NULL;
	plist_t description_node = NULL;
	crashreport_t* crashreport = NULL;

	// The description element is the one with all the good stuff
	description_node = plist_dict_get_item(plist, "description");
	if (description_node && plist_get_node_type(description_node) == PLIST_STRING) {
		plist_get_string_val(description_node, &description);

		crashreport = crashreport_create();
		if(crashreport == NULL) {
			error("Unable to allocate memory for crashreport\n");
			return NULL;
		}

		crashreport->state = crashreport_parse_state(description);
		if(crashreport->state == NULL) {
			error("Unable to parse ARM state from crashreport\n");
			crashreport_free(crashreport);
			return NULL;
		}

		crashreport->dylibs = crashreport_parse_dylibs(description);
		if(crashreport->dylibs == NULL) {
			error("Unable to parse dylib base addresses from crashreport\n");
			crashreport_free(crashreport);
			return NULL;
		}

		crashreport->threads = crashreport_parse_threads(description);
		if(crashreport->threads == NULL) {
			error("Unable to parse thread info from crashreport\n");
			crashreport_free(crashreport);
			return NULL;
		}
	}

	return crashreport;
}
