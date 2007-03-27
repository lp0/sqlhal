#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dict.h"
#include "types.h"
#include "db.h"
#include "output.h"

int do_list(const char *personality, const char *prefix, const char *type) {
	char *filename;
	int ret;

	filename = malloc((strlen(prefix) + 5) * sizeof(char));
	sprintf(filename, "%s.%s", prefix, type);

	ret = initialise_list(personality, type, filename);
	return ret;
}

int do_map(const char *personality, const char *prefix, const char *type) {
	char *filename;
	int ret;

	filename = malloc((strlen(prefix) + 5) * sizeof(char));
	sprintf(filename, "%s.%s", prefix, type);

	ret = initialise_map(personality, type, filename);
	return ret;
}

int main(int argc, char *argv[]) {
	char *prefix;
	int ret;
	char *state;

	if (argc < 2 || argc > 3) {
		printf("Brain loader\n");
		printf("Usage: %s <personality> [filename prefix]\n", argv[0]);
		return 1;
	}

	prefix = argv[argc - 1];

	state = "db_connect";
	ret = db_connect();
	if (ret) goto fail;
	else log_info("brain", ret, state);

	state = "db_begin";
	ret = db_begin();
	if (ret) goto fail;
	else log_info("brain", ret, state);

	state = "do_list aux";
	ret = do_list(argv[1], prefix, "aux");
	if (ret) log_warn("brain", ret, state);
	else log_info("brain", ret, state);

	state = "do_list ban";
	ret = do_list(argv[1], prefix, "ban");
	if (ret) log_warn("brain", ret, state);
	else log_info("brain", ret, state);

	state = "do_list grt";
	ret = do_list(argv[1], prefix, "grt");
	if (ret) log_warn("brain", ret, state);
	else log_info("brain", ret, state);

	state = "do_map swp";
	do_map(argv[1], prefix, "swp");
	if (ret) log_warn("brain", ret, state);
	else log_info("brain", ret, state);

	state = "db_commit";
	ret = db_commit();
	if (ret) goto fail;
	else log_info("brain", ret, state);

	state = "db_disconnect";
	ret = db_disconnect();
	if (ret) goto fail;
	else log_info("brain", ret, state);

	return 0;

fail:
	log_fatal("brain", ret, state);
	return 1;
}
