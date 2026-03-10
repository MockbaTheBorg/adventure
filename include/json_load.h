#ifndef JSON_LOAD_H
#define JSON_LOAD_H

#include "cJSON.h"

/*
 * json_load_file - Read a JSON file from disk and parse it.
 *
 * Returns a cJSON root node on success (caller must cJSON_Delete() it).
 * Prints an error to stderr and returns NULL on failure.
 */
cJSON *json_load_file(const char *path);

/*
 * json_get_string - Safely retrieve a string field from a JSON object.
 *
 * Returns the string value if the field exists and is a string,
 * or fallback if the field is missing or not a string.
 */
const char *json_get_string(const cJSON *obj, const char *key,
                            const char *fallback);

/*
 * json_get_int - Safely retrieve an integer field from a JSON object.
 *
 * Returns the integer value, or fallback if missing / wrong type.
 */
int json_get_int(const cJSON *obj, const char *key, int fallback);

/*
 * json_get_bool - Safely retrieve a boolean field from a JSON object.
 *
 * Returns 1 (true) or 0 (false), or fallback if missing / wrong type.
 */
int json_get_bool(const cJSON *obj, const char *key, int fallback);

#endif /* JSON_LOAD_H */
