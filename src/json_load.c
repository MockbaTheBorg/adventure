#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "json_load.h"

cJSON *json_load_file(const char *path)
{
    FILE   *fp;
    long    size;
    char   *buf;
    cJSON  *root;

    fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "error: cannot open '%s'\n", path);
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    rewind(fp);

    buf = (char *)malloc((size_t)size + 1);
    if (!buf) {
        fprintf(stderr, "error: out of memory reading '%s'\n", path);
        fclose(fp);
        return NULL;
    }

    if ((long)fread(buf, 1, (size_t)size, fp) != size) {
        fprintf(stderr, "error: failed to read '%s'\n", path);
        free(buf);
        fclose(fp);
        return NULL;
    }
    buf[size] = '\0';
    fclose(fp);

    root = cJSON_Parse(buf);
    free(buf);

    if (!root) {
        fprintf(stderr, "error: JSON parse error in '%s': %s\n",
                path, cJSON_GetErrorPtr());
        return NULL;
    }

    return root;
}

const char *json_get_string(const cJSON *obj, const char *key,
                             const char *fallback)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (item && cJSON_IsString(item) && item->valuestring)
        return item->valuestring;
    return fallback;
}

int json_get_int(const cJSON *obj, const char *key, int fallback)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (item && cJSON_IsNumber(item))
        return (int)item->valuedouble;
    return fallback;
}

int json_get_bool(const cJSON *obj, const char *key, int fallback)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!item) return fallback;
    if (cJSON_IsTrue(item))  return 1;
    if (cJSON_IsFalse(item)) return 0;
    return fallback;
}
