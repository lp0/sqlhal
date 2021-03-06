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

int db_word_add(const char *word, word_t *ref) {
	PGresult *res;
	const char *param[1];

	if (word == NULL || ref == NULL) return -EINVAL;
	if (db_connect()) return -EDB;

	param[0] = word;
	res = PQexecPrepared(conn, "word_add", 1, param, NULL, NULL, 1);
	if (PQresultStatus(res) != PGRES_COMMAND_OK) goto fail;
	PQclear(res);

	res = PQexecPrepared(conn, "word_add_id", 0, NULL, NULL, NULL, 0);
	if (PQresultStatus(res) != PGRES_TUPLES_OK) goto fail;
	if (PQntuples(res) != 1) goto fail;

	GET_VALUE(res, 0, 0, *ref);

	PQclear(res);

	return OK;

fail:
	log_error("db_word_add", PQresultStatus(res), PQresultErrorMessage(res));
	PQclear(res);
	return -EDB;
}

int db_word_get(const char *word, word_t *ref) {
	PGresult *res;
	const char *param[1];

	if (word == NULL) return -EINVAL;
	if (db_connect()) return -EDB;

	param[0] = word;
	res = PQexecPrepared(conn, "word_get", 1, param, NULL, NULL, 0);
	if (PQresultStatus(res) != PGRES_TUPLES_OK) goto fail;
	if (PQntuples(res) == 0) goto end;

	GET_VALUE(res, 0, 0, *ref);

	PQclear(res);

	return OK;

fail:
	log_error("db_word_get", PQresultStatus(res), PQresultErrorMessage(res));
	PQclear(res);
	return -EDB;

end:
	PQclear(res);
	return -ENOTFOUND;
}

int db_word_str(word_t ref, char **word) {
	PGresult *res;
	const char *param[1];
	char tmp[1][32];
	char *text;

	if (ref == 0 || word == NULL) return -EINVAL;
	if (db_connect()) return -EDB;

	SET_PARAM(param, tmp, 0, ref);

	res = PQexecPrepared(conn, "word_str", 1, param, NULL, NULL, 0);
	if (PQresultStatus(res) != PGRES_TUPLES_OK) goto fail;
	if (PQntuples(res) == 0) goto end;

	text = PQgetvalue(res, 0, 0);
	if (text == NULL) goto fail;

	*word = strdup(text);
	if (*word == NULL) {
		PQclear(res);
		return -ENOMEM;
	}

	PQclear(res);

	return OK;

fail:
	log_error("db_word_str", PQresultStatus(res), PQresultErrorMessage(res));
	PQclear(res);
	return -EDB;

end:
	PQclear(res);
	return -ENOTFOUND;
}
