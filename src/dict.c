#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "err.h"
#include "types.h"
#include "db.h"
#include "dict.h"

int load_list(const char *name, enum list type, const char *filename) {
	FILE *fd;
	char buffer[1024];
	char *string;
	int ret = OK;
	brain_t brain;

	WARN_IF(name == NULL);
	WARN_IF(filename == NULL);

	fd = fopen(filename, "r");
	if (fd == NULL) return -EIO;

	ret = db_brain_use(name, &brain);
	if (ret) goto fail;

	ret = db_list_zap(brain, type);
	if (ret) goto fail;

	while (!feof(fd)) {
		if (fgets(buffer, 1024, fd) == NULL) break;
		if (buffer[0] == '#') continue;
		string = strtok(buffer, "\t \r\n#");

		if ((string != NULL) && (strlen(string) > 0)) {
			word_t word;

			ret = db_word_use(string, &word);
			if (ret) goto fail;

			ret = db_list_contains(brain, type, word);
			if (ret == -ENOTFOUND)
				ret = db_list_add(brain, type, word);
			if (ret) goto fail;
		}
	}

fail:
	fclose(fd);
	return ret;
}

int iter_list_item(void *data, word_t ref, const char *word) {
	FILE *fd = data;
	int ret;

	BUG_IF(word == NULL);
	BUG_IF(ref == 0);
	BUG_IF(word[0] == 0);

	ret = fprintf(fd, "%s\n", word);
	if (ret <= 0) return -EIO;

	return OK;
}

int save_list(const char *name, enum list type, const char *filename) {
	FILE *fd;
	int ret = OK;
	brain_t brain;

	WARN_IF(name == NULL);
	WARN_IF(filename == NULL);

	fd = fopen(filename, "w");
	if (fd == NULL) return -EIO;

	switch (type) {
	case LIST_AUX:
		ret = fprintf(fd, "#\n"\
			"#\tThis is a list of words which can be used as keywords only\n"\
			"#\tin order to supplement other keywords\n"\
			"#\n");
		break;
	case LIST_BAN:
		ret = fprintf(fd, "#\n"\
			"#\tThis is a list of words which cannot be used as keywords\n"\
			"#\n");
		break;
	case LIST_GREET:
		ret = fprintf(fd, "#\n"\
			"#\tThis is a list of words which will be used as keywords whenever\n"\
			"#\tthe judge changes, in order to greet them.\n"\
			"#\n");
		break;
	default:
		fclose(fd);
		BUG();
	}

	if (ret <= 0) {
		ret = -EIO;
		goto fail;
	}

	ret = db_brain_get(name, &brain);
	if (ret) goto fail;

	ret = db_list_iter(brain, type, iter_list_item, fd);
	if (ret) goto fail;

	if (fclose(fd) != 0)
		return -EIO;
	return OK;

fail:
	fclose(fd);
	return ret;
}

int load_map(const char *name, enum map type, const char *filename) {
	FILE *fd;
	char buffer[1024];
	char *from;
	char *to;
	int ret = OK;
	brain_t brain;

	WARN_IF(name == NULL);
	WARN_IF(filename == NULL);

	fd = fopen(filename, "r");
	if (fd == NULL) return -EIO;

	ret = db_brain_use(name, &brain);
	if (ret) goto fail;

	ret = db_map_zap(brain, type);
	if (ret) goto fail;

	while (!feof(fd)) {
		if (fgets(buffer, 1024, fd) == NULL) break;
		if (buffer[0] == '#') continue;

		from = strtok(buffer, "\t ");
		to = strtok(NULL, "\t \r\n#");

		if ((from != NULL) && (strlen(from) > 0) && (to != NULL) && (strlen(to) > 0)) {
			word_t key, value;

			ret = db_word_use(from, &key);
			if (ret) goto fail;

			ret = db_word_use(to, &value);
			if (ret) goto fail;

			ret = db_map_get(brain, type, key, &value);
			if (ret == -ENOTFOUND)
				ret = db_map_put(brain, type, key, value);
			if (ret) goto fail;
		}
	}

fail:
	fclose(fd);
	return ret;
}

int iter_map_item(void *data, word_t key_ref, word_t value_ref, const char *key, const char *value) {
	FILE *fd = data;
	int ret;

	BUG_IF(key == NULL);
	BUG_IF(key[0] == 0);
	BUG_IF(key_ref == 0);
	BUG_IF(value == NULL);
	BUG_IF(value[0] == 0);
	BUG_IF(value_ref == 0);

	ret = fprintf(fd, "%s\t%s\n", key, value);
	if (ret <= 0) return -EIO;

	return OK;
}

int save_map(const char *name, enum map type, const char *filename) {
	FILE *fd;
	int ret = OK;
	brain_t brain;

	WARN_IF(name == NULL);
	WARN_IF(filename == NULL);

	fd = fopen(filename, "w");
	if (fd == NULL) return -EIO;

	switch (type) {
	case MAP_SWAP:
		ret = fprintf(fd, "#\n"\
			"#\tThe word on the left is changed to the word on the\n"\
			"#\tright when used as a keyword\n"\
			"#\n");
		break;
		break;
	default:
		fclose(fd);
		BUG();
	}

	if (ret <= 0) {
		ret = -EIO;
		goto fail;
	}

	ret = db_brain_get(name, &brain);
	if (ret) goto fail;

	ret = db_map_iter(brain, type, iter_map_item, fd);
	if (ret) goto fail;

	if (fclose(fd) != 0)
		return -EIO;
	return OK;

fail:
	fclose(fd);
	return ret;
}

dict_t *dict_alloc(void) {
	dict_t *dict;

	dict = malloc(sizeof(dict_t));
	if (dict == NULL) return NULL;

	dict->size = 0;
	dict->words = NULL;

	return dict;
}

int dict_add(dict_t *dict, word_t word, uint32_t *pos) {
	uint_fast32_t i;
	uint32_t tmp;
	void *mem;
	int ret;

	WARN_IF(dict == NULL);
	WARN_IF(word == 0);
	if (pos == NULL) pos = &tmp;

	ret = dict_find(dict, word, pos);
	if (ret != -ENOTFOUND) return ret;

	if (dict->size >= UINT32_MAX)
		return -ENOSPC;

	dict->size++;

	BUG_IF(dict->size <= 0);

	mem = realloc(dict->words, sizeof(word_t) * dict->size);
	if (mem == NULL) { dict->size--; return -ENOMEM; }
	dict->words = mem;

	for (i = dict->size - 1; i > *pos; i--)
		dict->words[i] = dict->words[i - 1];
	dict->words[*pos] = word;

	return OK;
}

int dict_del(dict_t *dict, word_t word, uint32_t *pos) {
	uint_fast32_t i;
	uint32_t tmp;
	void *mem;
	int ret;

	WARN_IF(dict == NULL);
	WARN_IF(word == 0);
	if (pos == NULL) pos = &tmp;

	ret = dict_find(dict, word, pos);
	if (ret != OK) return ret;

	dict->size--;

	for (i = *pos; i < dict->size; i++)
		dict->words[i] = dict->words[i + 1];

	mem = realloc(dict->words, sizeof(word_t) * dict->size);
	if (mem == NULL) return -ENOMEM;
	dict->words = mem;

	return OK;
}

int dict_get(const dict_t *dict, uint32_t pos, word_t *word) {
	WARN_IF(dict == NULL);
	WARN_IF(word == NULL);
	if (pos >= dict->size) return -ENOTFOUND;
	*word = dict->words[pos];
	return OK;
}

int dict_size(const dict_t *dict, uint32_t *size) {
	WARN_IF(dict == NULL);
	WARN_IF(size == NULL);
	*size = dict->size;
	return OK;
}

int dict_find(const dict_t *dict, word_t word, uint32_t *pos) {
	uint_fast32_t min = 0;
	uint_fast32_t tmp;
	uint_fast32_t max;
	int ret;

	WARN_IF(dict == NULL);
	WARN_IF(word == 0);

	if (dict->size == 0) {
		if (pos != NULL)
			*pos = 0;
		return -ENOTFOUND;
	}
	max = dict->size - 1;

	while (1) {
		tmp = (min + max) / 2;

		if (word == dict->words[tmp]) {
			ret = OK;
			goto done;
		} else if (word > dict->words[tmp]) {
			if (max == tmp) {
				ret = -ENOTFOUND;
				tmp++;
				goto done;
			}
			min = tmp + 1;
		} else {
			if (min == tmp) {
				ret = -ENOTFOUND;
				goto done;
			}
			max = tmp - 1;
		}
	}

done:
	if (pos != NULL)
		*pos = tmp;
	return ret;
}

void dict_free(dict_t **dict) {
	dict_t *dict_p;

	if (*dict == NULL) return;
	dict_p = *dict;

	free(dict_p->words);
	free(*dict);
	*dict = NULL;
}

list_t *list_alloc(void) {
	list_t *list;

	list = malloc(sizeof(list_t));
	if (list == NULL) return NULL;

	list->size = 0;
	list->words = NULL;

	return list;
}

int list_append(list_t *list, word_t word) {
	void *mem;

	WARN_IF(list == NULL);
	WARN_IF(word == 0);

	if (list->size >= UINT32_MAX)
		return -ENOSPC;

	list->size++;

	BUG_IF(list->size <= 0);

	mem = realloc(list->words, sizeof(word_t) * list->size);
	if (mem == NULL) { list->size--; return -ENOMEM; }
	list->words = mem;

	list->words[list->size - 1] = word;
	return OK;
}

int list_prepend(list_t *list, word_t word) {
	uint_fast32_t i;	
	void *mem;

	WARN_IF(list == NULL);
	WARN_IF(word == 0);

	if (list->size >= UINT32_MAX)
		return -ENOSPC;

	list->size++;

	BUG_IF(list->size <= 0);

	mem = realloc(list->words, sizeof(word_t) * list->size);
	if (mem == NULL) { list->size--; return -ENOMEM; }
	list->words = mem;

	for (i = 1; i < list->size; i++)
		list->words[i] = list->words[i - 1];

	list->words[0] = word;
	return OK;
}

int list_get(const list_t *list, uint32_t pos, word_t *word) {
	WARN_IF(list == NULL);
	WARN_IF(word == NULL);
	if (pos >= list->size) return -ENOTFOUND;
	*word = list->words[pos];
	return OK;
}

int list_set(list_t *list, uint32_t pos, word_t word) {
	WARN_IF(list == NULL);
	WARN_IF(word == 0);
	if (pos >= list->size) return -ENOTFOUND;
	list->words[pos] = word;
	return OK;
}

int list_size(const list_t *list, uint32_t *size) {
	WARN_IF(list == NULL);
	WARN_IF(size == NULL);
	*size = list->size;
	return OK;
}

int list_equal(const list_t *a, const list_t *b) {
	uint_fast32_t i;

	WARN_IF(a == NULL);
	WARN_IF(b == NULL);

	if (a->size != b->size)
		return 0;

	for (i = 0; i < a->size; i++)
		if (a->words[i] != b->words[i])
			return 0;

	return 1;
}

int list_contains(const list_t *list, word_t word) {
	uint_fast32_t i;

	WARN_IF(list == NULL);

	for (i = 0; i < list->size; i++)
		if (list->words[i] == word)
			return OK;

	return -ENOTFOUND;
}

void list_free(list_t **list) {
	list_t *list_p;

	if (*list == NULL) return;
	list_p = *list;

	free(list_p->words);
	free(*list);
	*list = NULL;
}

