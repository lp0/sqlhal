#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libpq-fe.h>

#include "err.h"
#include "types.h"
#include "db.h"
#include "output.h"

#include "db_postgres.h"

int db_model_get_order(brain_t brain, number_t *order) {
	PGresult *res;
	const char *param[1];
	char tmp[1][32];

	WARN_IF(brain == 0);
	WARN_IF(order == NULL);
	if (db_connect())
		return -EDB;

	SET_PARAM(param, tmp, 0, brain);

	res = PQexecPrepared(conn, "model_get", 1, param, NULL, NULL, 0);
	if (PQresultStatus(res) != PGRES_TUPLES_OK) goto fail;
	if (PQntuples(res) == 0) goto not_found;

	GET_VALUE(res, 0, 0, *order);

	PQclear(res);

	return OK;

fail:
	log_error("db_model_get_order", PQresultStatus(res), PQresultErrorMessage(res));
	PQclear(res);
	return -EDB;

not_found:
	PQclear(res);
	return -ENOTFOUND;
}

int db_model_set_order(brain_t brain, number_t order) {
	PGresult *res;
	const char *param[2];
	char tmp[2][32];
	number_t current;
	int ret;

	WARN_IF(brain == 0);
	WARN_IF(order == 0);
	if (db_connect())
		return -EDB;

	SET_PARAM(param, tmp, 0, brain);
	SET_PARAM(param, tmp, 1, order);

	ret = db_model_get_order(brain, &current);
	if (ret == -ENOTFOUND) {
		res = PQexecPrepared(conn, "model_add", 2, param, NULL, NULL, 0);
		if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
		PQclear(res);
	} else if (!ret) {
		res = PQexecPrepared(conn, "model_set", 2, param, NULL, NULL, 0);
		if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
		PQclear(res);
	} else {
		return ret;
	}

	return OK;

fail:
	log_error("db_model_set_order", PQresultStatus(res), PQresultErrorMessage(res));
	PQclear(res);
	return -EDB;
}

int db_model_zap(brain_t brain) {
	PGresult *res;
	const char *param[1];
	char tmp[1][32];

	WARN_IF(brain == 0);
	if (db_connect())
		return -EDB;

	SET_PARAM(param, tmp, 0, brain);

	res = PQexecPrepared(conn, "model_zap", 1, param, NULL, NULL, 0);
	if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
	PQclear(res);

	return OK;

fail:
	log_error("db_model_zap", PQresultStatus(res), PQresultErrorMessage(res));
	PQclear(res);
	return -EDB;
}

int db_model_node_fill(brain_t brain, db_tree *node) {
	PGresult *res;
	unsigned int num, pos, i;
	const char *param[2];
	char tmp[2][32];
	int found;

	WARN_IF(brain == 0);
	WARN_IF(node == NULL);
	WARN_IF(node->id == 0);
	if (db_connect())
		return -EDB;

	SET_PARAM(param, tmp, 0, brain);
	SET_PARAM(param, tmp, 1, node->id);

	res = PQexecPrepared(conn, "model_node_get", 2, param, NULL, NULL, 0);
	if (PQresultStatus(res) != PGRES_TUPLES_OK) goto fail;

	num = PQntuples(res);
	if (num == 0) goto not_found;

	if (node->nodes != NULL) {
		for (i = 0; i < node->children; i++)
			db_model_node_free((db_tree **)&node->nodes[i++]);
		free(node->nodes);
		node->children = 0;
	}

	node->children = num - 1;

	if (node->children > 0) {
		node->nodes = malloc(sizeof(db_tree *) * node->children);
		if (node->nodes == NULL) {
			PQclear(res);

			node->children = 0;

			return -ENOMEM;
		}
	}

	found = 0;
	for (i = 0, pos = 0; i < num; i++) {
		db_tree *child;
		node_t id;

		GET_VALUE(res, i, 0, id);

		if (id == node->id) {
			found = 1;

			GET_VALUE(res, i, 1, node->word);
			GET_VALUE(res, i, 2, node->usage);
			GET_VALUE(res, i, 3, node->count);
		} else {
			node->nodes[pos] = db_model_node_alloc();
			if (node->nodes[pos] == NULL) {
				PQclear(res);

				while (pos > 0)
					db_model_node_free((db_tree **)&node->nodes[--pos]);
				free(node->nodes);
				node->nodes = NULL;

				return -ENOMEM;
			}

			child = (db_tree *)node->nodes[pos];

			child->id = id;
			child->parent_id = node->id;
			GET_VALUE(res, i, 1, child->word);
			GET_VALUE(res, i, 2, child->usage);
			GET_VALUE(res, i, 3, child->count);

			pos++;
		}
	}
	PQclear(res);

	BUG_IF(!found);
	return OK;

fail:
	log_error("db_model_node_fill", PQresultStatus(res), PQresultErrorMessage(res));
	PQclear(res);
	return -EDB;

not_found:
	log_error("db_model_node_fill", node->id, "Node not found");
	PQclear(res);
	return -ENOTFOUND;
}

int db_model_node_find(brain_t brain, db_tree *tree, word_t word, db_tree **found) {
	PGresult *res;
	const char *param[3];
	char tmp[3][32];
	int ret;
	db_tree *found_p;

	WARN_IF(brain == 0);
	WARN_IF(tree == NULL);
	WARN_IF(tree->id == 0);
	WARN_IF(found == NULL);
	if (db_connect())
		return -EDB;

	SET_PARAM(param, tmp, 0, brain);
	SET_PARAM(param, tmp, 1, tree->id);
	if (word == 0)
		param[2] = NULL;
	else
		SET_PARAM(param, tmp, 2, word);

	res = PQexecPrepared(conn, "model_node_find", 3, param, NULL, NULL, 0);
	if (PQresultStatus(res) != PGRES_TUPLES_OK) goto fail;
	if (PQntuples(res) == 0) goto not_found;

	if (*found != NULL) {
		ret = db_model_node_clear(*found);
		if (ret) {
			PQclear(res);
			return ret;
		}
	} else {
		*found = db_model_node_alloc();
		if (*found == NULL) {
			PQclear(res);
			return -ENOMEM;
		}
	}
	found_p = *found;

	found_p->parent_id = tree->id;
	GET_VALUE(res, 0, 0, found_p->id);
	GET_VALUE(res, 0, 1, found_p->word);
	GET_VALUE(res, 0, 2, found_p->usage);
	GET_VALUE(res, 0, 3, found_p->count);

	PQclear(res);
	return OK;

fail:
	log_error("db_model_node_find", PQresultStatus(res), PQresultErrorMessage(res));
	PQclear(res);
	return -EDB;

not_found:
	PQclear(res);
	return -ENOTFOUND;
}

int db_model_get_root(brain_t brain, db_tree **forward, db_tree **backward) {
	PGresult *res;
	const char *param[3];
	char tmp[3][32];
	db_tree *forward_p;
	db_tree *backward_p;
	int created = 0;
	int ret;

	WARN_IF(brain == 0);
	WARN_IF(forward == NULL);
	WARN_IF(backward == NULL);
	if (db_connect())
		return -EDB;

	*forward = NULL;
	*backward = NULL;

	SET_PARAM(param, tmp, 0, brain);

	res = PQexecPrepared(conn, "model_root_get", 1, param, NULL, NULL, 0);
	if (PQresultStatus(res) != PGRES_TUPLES_OK) goto fail;
	if (PQntuples(res) == 0) goto fail;

	if (!PQgetisnull(res, 0, 0)) {
		*forward = db_model_node_alloc();
		if (*forward == NULL) return -ENOMEM;
		forward_p = *forward;

		GET_VALUE(res, 0, 0, forward_p->id);
	} else {
		ret = db_model_create(brain, forward);
		if (ret) { PQclear(res); return ret; }
		forward_p = *forward;

		created = 1;
	}

	if (!PQgetisnull(res, 0, 1)) {
		*backward = db_model_node_alloc();
		if (*backward == NULL) return -ENOMEM;
		backward_p = *backward;

		GET_VALUE(res, 0, 1, backward_p->id);
	} else {
		ret = db_model_create(brain, backward);
		if (ret) { PQclear(res); return ret; }
		backward_p = *backward;

		created = 1;
	}
	PQclear(res);

	if (created) {
		SET_PARAM(param, tmp, 1, forward_p->id);
		SET_PARAM(param, tmp, 2, backward_p->id);

		res = PQexecPrepared(conn, "model_root_set", 3, param, NULL, NULL, 0);
		if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
		PQclear(res);
	}

	return OK;

fail:
	log_error("db_model_get_root", PQresultStatus(res), PQresultErrorMessage(res));
	db_model_node_free(forward);
	db_model_node_free(backward);
	PQclear(res);
	return -EDB;
}

int db_model_create(brain_t brain, db_tree **node) {
	PGresult *res;
	const char *param[1];
	char tmp[1][32];
	db_tree *node_p;

	WARN_IF(brain == 0);
	WARN_IF(node == NULL);
	if (db_connect())
		return -EDB;

	*node = db_model_node_alloc();
	if (*node == NULL) return -ENOMEM;
	node_p = *node;

	SET_PARAM(param, tmp, 0, brain);

	res = PQexecPrepared(conn, "model_create", 1, param, NULL, NULL, 0);
	if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
	PQclear(res);

	res = PQexecPrepared(conn, "model_create_id", 0, NULL, NULL, NULL, 0);
	if (PQresultStatus(res) != PGRES_TUPLES_OK) goto fail;
	if (PQntuples(res) != 1) goto fail;

	GET_VALUE(res, 0, 0, node_p->id);

	PQclear(res);

	return OK;

fail:
	log_error("db_model_create", PQresultStatus(res), PQresultErrorMessage(res));
	free(*node);
	PQclear(res);
	return -EDB;
}

int db_model_update(brain_t brain, db_tree *node) {
	PGresult *res;
	const char *param[5];
	char tmp[5][32];

	WARN_IF(brain == 0);
	WARN_IF(node == NULL);
	WARN_IF(node->parent_id == 0 && node->word != 0);
	if (db_connect())
		return -EDB;

	if (node->id == 0) {
		SET_PARAM(param, tmp, 0, brain);
	} else {
		SET_PARAM(param, tmp, 0, node->id);
	}

	SET_PARAM(param, tmp, 1, node->usage);
	SET_PARAM(param, tmp, 2, node->count);

	if (node->parent_id == 0) {
		res = PQexecPrepared(conn, "model_rootupdate", 3, param, NULL, NULL, 0);
		if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
		PQclear(res);
	} else if (node->id == 0) {
		if (node->word == 0) {
			param[3] = NULL;
		} else {
			SET_PARAM(param, tmp, 3, node->word);
		}
		SET_PARAM(param, tmp, 4, node->parent_id);

		res = PQexecPrepared(conn, "model_fastcreate", 5, param, NULL, NULL, 0);
		if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
		PQclear(res);

		res = PQexecPrepared(conn, "model_create_id", 0, NULL, NULL, NULL, 0);
		if (PQresultStatus(res) != PGRES_TUPLES_OK) goto fail;
		if (PQntuples(res) != 1) goto fail;

		GET_VALUE(res, 0, 0, node->id);

		PQclear(res);
	} else {
		res = PQexecPrepared(conn, "model_update", 3, param, NULL, NULL, 0);
		if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
		PQclear(res);
	}

	return OK;

fail:
	log_error("db_model_update", PQresultStatus(res), PQresultErrorMessage(res));
	PQclear(res);
	return -EDB;
}

int db_model_link(db_tree *parent, db_tree *child) {
	BUG_IF(parent == NULL);
	BUG_IF(child == NULL);
	BUG_IF(parent->id == 0);
	BUG_IF(child->parent_id != 0);
	BUG_IF(parent->parent_id != 0 && parent->word == 0);

	child->parent_id = parent->id;
	return OK;
}

int db_model_contains(brain_t brain, word_t word) {
	PGresult *res;
	const char *param[2];
	char tmp[2][32];

	WARN_IF(brain == 0);
	WARN_IF(word == 0);
	if (db_connect())
		return -EDB;

	SET_PARAM(param, tmp, 0, brain);
	SET_PARAM(param, tmp, 1, word);

	res = PQexecPrepared(conn, "model_word_exists", 2, param, NULL, NULL, 0);
	if (PQresultStatus(res) != PGRES_TUPLES_OK) goto fail;
	if (PQntuples(res) == 0) goto not_found;

	PQclear(res);
	return OK;

fail:
	log_error("db_model_contains", PQresultStatus(res), PQresultErrorMessage(res));
	PQclear(res);
	return -EDB;

not_found:
	PQclear(res);
	return -ENOTFOUND;
}

int db_model_rand_word(brain_t brain, const db_tree *node, word_t *word) {
	PGresult *res;
	const char *param[2];
	char tmp[2][32];

	WARN_IF(brain == 0);
	WARN_IF(node == NULL);
	WARN_IF(node->id == 0);
	if (db_connect())
		return -EDB;

	SET_PARAM(param, tmp, 0, brain);
	SET_PARAM(param, tmp, 1, node->id);

	res = PQexecPrepared(conn, "model_word_random", 2, param, NULL, NULL, 0);
	if (PQresultStatus(res) != PGRES_TUPLES_OK) goto fail;
	if (PQntuples(res) == 0) goto not_found;

	GET_VALUE(res, 0, 0, *word);

	PQclear(res);
	return OK;

fail:
	log_error("db_model_rand_word", PQresultStatus(res), PQresultErrorMessage(res));
	PQclear(res);
	return -EDB;

not_found:
	PQclear(res);
	return -ENOTFOUND;
}

int db_model_rand_node(brain_t brain, const db_tree *parent, db_tree **node) {
	PGresult *res;
	const char *param[2];
	char tmp[2][32];
	db_tree *node_p;

	WARN_IF(brain == 0);
	WARN_IF(parent == NULL);
	WARN_IF(parent->id == 0);
	WARN_IF(node == NULL);
	if (db_connect())
		return -EDB;

	SET_PARAM(param, tmp, 0, brain);
	SET_PARAM(param, tmp, 1, parent->id);

	res = PQexecPrepared(conn, "model_node_random", 2, param, NULL, NULL, 0);
	if (PQresultStatus(res) != PGRES_TUPLES_OK) goto fail;
	if (PQntuples(res) == 0) goto not_found;

	*node = db_model_node_alloc();
	if (*node == NULL) {
		PQclear(res);
		return -ENOMEM;
	}
	node_p = *node;

	node_p->parent_id = parent->id;
	GET_VALUE(res, 0, 0, node_p->id);
	GET_VALUE(res, 0, 1, node_p->word);
	GET_VALUE(res, 0, 2, node_p->usage);
	GET_VALUE(res, 0, 3, node_p->count);

	PQclear(res);
	return OK;

fail:
	log_error("db_model_rand_node", PQresultStatus(res), PQresultErrorMessage(res));
	PQclear(res);
	return -EDB;

not_found:
	PQclear(res);
	return -ENOTFOUND;
}

int db_model_next_node(brain_t brain, const db_tree *current, db_tree **next) {
	PGresult *res;
	const char *param[3];
	char tmp[3][32];
	db_tree *node_p;
	int ret;

	WARN_IF(brain == 0);
	WARN_IF(current == NULL);
	WARN_IF(current->id == 0);
	WARN_IF(current->parent_id == 0);
	WARN_IF(next == NULL);
	if (db_connect())
		return -EDB;

	SET_PARAM(param, tmp, 0, brain);
	SET_PARAM(param, tmp, 1, current->parent_id);
	SET_PARAM(param, tmp, 2, current->id);

	res = PQexecPrepared(conn, "model_node_next", 3, param, NULL, NULL, 0);
	if (PQresultStatus(res) != PGRES_TUPLES_OK) goto fail;
	if (PQntuples(res) == 0) {
		PQclear(res);

		res = PQexecPrepared(conn, "model_node_first", 2, param, NULL, NULL, 0);
		if (PQresultStatus(res) != PGRES_TUPLES_OK) goto fail;
		if (PQntuples(res) == 0) goto not_found;
	}

	if (*next != NULL) {
		ret = db_model_node_clear(*next);
		if (ret) {
			PQclear(res);
			return ret;
		}
	} else {
		*next = db_model_node_alloc();
		if (*next == NULL) {
			PQclear(res);
			return -ENOMEM;
		}
	}
	node_p = *next;

	GET_VALUE(res, 0, 0, node_p->id);
	GET_VALUE(res, 0, 1, node_p->parent_id);
	GET_VALUE(res, 0, 2, node_p->word);
	GET_VALUE(res, 0, 3, node_p->usage);
	GET_VALUE(res, 0, 4, node_p->count);

	PQclear(res);
	return OK;

fail:
	log_error("db_model_next_node", PQresultStatus(res), PQresultErrorMessage(res));
	PQclear(res);
	return -EDB;

not_found:
	PQclear(res);
	return -ENOTFOUND;
}

int db_model_dump_words(brain_t brain, int (*allocate)(void *data, number_t size), int (*callback)(void *data, word_t word, number_t index, const char *text), void *data) {
	PGresult *res;
	unsigned int num, i;
	const char *param[1];
	char tmp[1][32];
	int ret;

	WARN_IF(brain == 0);
	WARN_IF(data == NULL);
	WARN_IF(callback == NULL);
	if (db_connect())
		return -EDB;

	SET_PARAM(param, tmp, 0, brain);

	res = PQexecPrepared(conn, "model_brain_words", 1, param, NULL, NULL, 0);
	if (PQresultStatus(res) != PGRES_TUPLES_OK) goto fail;

	num = PQntuples(res);
	if (allocate != NULL) {
		ret = allocate(data, num);
		if (ret) return ret;
	}

	for (i = 0; i < num; i++) {
		word_t word;
		number_t pos;
		char *text;

		GET_VALUE(res, i, 0, word);
		GET_VALUE(res, i, 1, pos);
		text = PQgetvalue(res, i, 2);
		if (text == NULL) goto fail;

		ret = callback(data, word, pos, text);
		if (ret) {
			PQclear(res);
			return ret;
		}
	}

	PQclear(res);
	return OK;

fail:
	log_error("db_model_dump_words", PQresultStatus(res), PQresultErrorMessage(res));
	PQclear(res);
	return -EDB;
}
