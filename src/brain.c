#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dict.h"
#include "types.h"
#include "db.h"

void do_list(const char *personality, const char *prefix, const char *type) {
	char *filename;
	int ret;

	filename = malloc((strlen(prefix) + 5) * sizeof(char));
	sprintf(filename, "%s.%s", prefix, type);

	ret = initialise_list(personality, type, filename);
	if (ret) {
		printf("initialise_list failed for %s (err %d)\n", filename, ret);
		exit(1);
	}
}

int main(int argc, char *argv[]) {
	char *prefix;

	if (argc < 2 || argc > 3) {
		printf("Usage: %s <personality> [filename prefix]\n", argv[0]);
		return 1;
	}

	prefix = argv[argc - 1];

	do_list(argv[1], prefix, "aux");
	do_list(argv[1], prefix, "ban");
	do_list(argv[1], prefix, "grt");
//	do_list2("swp");

	db_disconnect();

	return 0;
}
