/*
 * Copyright (c), Recep Aslantas.
 *
 * MIT License (MIT), http://opensource.org/licenses/MIT
 * Full license can be found in the LICENSE file
 */

#ifndef json_impl_json_h
#define json_impl_json_h

#include "../json.h"
#include "impl_mem.h"

JSON_INLINE
const void*
json__impl_key(const char ** __restrict ptr, int * __restrict keysize) {
  const char *pi, *end, *start;
  char        c;

  pi = *ptr;
  c  = *pi;

  while (c == '\"' || c == '\'' || c == ' ' || c == ',')
    c = *++pi;

  start = end = pi;

  while (c != ':' && c != '\"' && c != '\'' && c != '\0') {
    if (c != ' '
        && c != '\r'
        && c != '\n'
        && c != '\t')
      end = pi + 1;
  
    c = *++pi;
  }

  *keysize = (int)(end - start);
  *ptr     = pi;

  return start;
}

JSON_INLINE
void*
json__impl_value(const char ** __restrict ptr, int * __restrict valuesize) {
  const char *pi, *end, *start;
  char        c;

  pi  = *ptr;
  c   = *pi;

  while (c == ':' || c == '\"' || c == '\'' || c == ' ')
    c = *++pi;

  start = end = pi;

  while (c != '\"'
         && c != '\''
         && c != ','
         && c != '{'
         && c != '}'
         && c != '['
         && c != ']'
         && c != '\0') {
    if (c != ' '
        && c != '\r'
        && c != '\n'
        && c != '\t')
      end = pi + 1;
    
    c = *++pi;
  }

  *valuesize = (int)(end - start);
  *ptr       = pi;

  return (void *)start;
}

JSON_INLINE
json_doc_t*
json_parse(const char * __restrict contents, bool reverse) {
  json_doc_t *doc;
  json_t     *obj, *val, *parent;
  const char *key;
  json_t      tmproot;
  int         keysize;
  char        c;
  bool        lookingForKey;

  if (!contents || (c = *contents) == '\0')
    return NULL;

  doc            = calloc(1, sizeof(*doc));
  doc->memroot   = calloc(1, sizeof(json_mem_t) + JSON_MEM_PAGE);
  doc->ptr       = contents;

  memset(&tmproot, 0, sizeof(tmproot));
  tmproot.type   = JSON_OBJECT;

  key            = NULL;
  parent         = &tmproot;
  obj            = parent;
  keysize        = 0;
  lookingForKey  = false;

  ((json_mem_t *)doc->memroot)->capacity = JSON_MEM_PAGE;

  do {
  again:
    /* child */
    switch (c) {
        /* trim */
      case ' ':
      case '\r':
      case '\n':
      case '\t':
      case '\'':
      case '"':
        break;
      case '{':
      case '[': {
        if (obj->type == JSON_ARRAY)
          ++((json_array_t *)obj)->count;

        /* switch parent */
        parent = obj;

        if (c == '{') {
          obj           = json__impl_calloc(doc, sizeof(json_t));
          obj->type     = JSON_OBJECT;
          lookingForKey = true;
        } else {
          obj       = json__impl_calloc(doc, sizeof(json_array_t));
          obj->type = JSON_ARRAY;
        }

        /* parent must not be NULL */

        if (!reverse) {
          if (!parent->next)
            parent->next = obj;
          else
            json_json(parent)->next = obj;
        } else {
          obj->next = parent->value;
        }

        parent->value = obj;
        obj->parent   = parent;

        if (key) {
          obj->key     = key;
          obj->keySize = keysize;
          key          = NULL;
        }

        break;
      }
      case '}':
      case ']':
        /* switch parent back */
        if (!parent)
          goto err;

        if (!reverse) {
          obj->value = obj->next;
          obj->next  = NULL;
        }

        obj           = parent;
        parent        = parent->parent;
        lookingForKey = obj->type == JSON_OBJECT;

        break;
      case ',': {
        lookingForKey = obj->type == JSON_OBJECT;
        break;
      }
      default: {
        /* looking for key */
        if (lookingForKey) {
          key = json__impl_key(&doc->ptr, &keysize);
          if (key == NULL || ((c = *key) == '\0'))
            goto err;

          lookingForKey = false;
          c             = *doc->ptr;

          /* jump to value */
          for (;;) {
            switch (c) {
              case ' ':
              case '\t':
              case '\n':
              case '\r':
              case '"':
              case '\'':
              case ':':
                c = *++doc->ptr;
                continue;
              default:
                goto val;
            }
          }

        val:
          goto again;
        }

        /* looking for value */
        else {
          val       = json__impl_calloc(doc, sizeof(json_t));
          val->type = JSON_STRING;

          if (obj->type == JSON_ARRAY)
            ++((json_array_t *)obj)->count;

          /* parent must not be NULL */

          if (!reverse) {
            if (!obj->next)
              obj->next = val;
             else
              json_json(obj)->next = val;
          } else {
            val->next = obj->value;
          }

          obj->value  = val;
          val->parent = obj;

          if (key) {
            val->key     = key;
            val->keySize = keysize;
            key          = NULL;
          }

          val->value = json__impl_value(&doc->ptr, &val->valSize);
          c          = *doc->ptr;

          goto again;
        } /* if lookingForKey */
      } /* switch->default */
    } /* switch */
  } while ((c = *doc->ptr) != '\0' && (c = *++doc->ptr) != '\0');

err:
  if (tmproot.value)
    ((json_t *)tmproot.value)->parent = NULL;

  doc->root = tmproot.value;
  return doc;
}

JSON_INLINE
void
json_free(json_doc_t * __restrict jsondoc) {
  json_mem_t *mem, *tofree;

  mem = jsondoc->memroot;
  while (mem) {
    tofree = mem;
    mem    = mem->next;
    free(tofree);
  }

  free(jsondoc);
}

JSON_INLINE
json_t*
json_get(const json_t * __restrict object, const char * __restrict key) {
  json_t *iter;
  size_t  keysize;
  
  if (!object || !key || !(iter = object->value))
    return NULL;
  
  keysize = strlen(key);
  while (iter && strncmp(iter->key, key, keysize) != 0)
    iter = iter->next;
  
  return iter;
}

JSON_INLINE
json_array_t*
json_array(const json_t * __restrict object) {
  if (object->type != JSON_ARRAY)
    return NULL;
  
  return (void *)object;
}

JSON_INLINE
int
json_count(const json_t * __restrict object) {
  const json_array_t *arr;

  if ((arr = json_array(object)))
    return arr->count;

  return 0;
}

JSON_INLINE
bool
json_is_array(const json_t * __restrict object) {
  return object->type == JSON_ARRAY;
}

JSON_INLINE
void
json_array_float(float        * __restrict dest,
                 const json_t * __restrict object,
                 float                     defaultValue,
                 int                       maxCount,
                 bool                      sourceIsReversed) {
  json_array_t *arr;
  json_t       *item;
  int           count, i;

  if (!(arr = json_array(object)))
    return;

  count = arr->count;
  item  = arr->base.value;

  if (maxCount > 0 && count > maxCount)
    count = maxCount;

  if (sourceIsReversed) {
    while (item) {
      if (count <= 0)
        break;

      dest[--count] = json_float(item, defaultValue);
      item          = item->next;
    }
  } else {
    i = 0;
    while (item) {
      if (i >= count)
        break;

      dest[i++] = json_float(item, defaultValue);
      item      = item->next;
    }
  }
}

#endif /* json_impl_json_h */
