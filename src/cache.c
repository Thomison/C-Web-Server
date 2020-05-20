#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hashtable.h"
#include "cache.h"

/**
 * Allocate a cache entry
 */
struct cache_entry *alloc_entry(char *path, char *content_type, void *content, int content_length)
{
    struct cache_entry *ret = calloc(1, sizeof(struct cache_entry));

    ret->path = strdup(path); 
    ret->content_type = strdup(content_type);
    ret->content_length = content_length;
    ret->content = strdup(content);
    ret->prev = NULL;
    ret->next = NULL;

    return ret;
}

/**
 * Deallocate a cache entry
 */
void free_entry(struct cache_entry *entry)
{
    free(entry->path);
    free(entry->content_type);
    free(entry->content);

    free(entry);
}

/**
 * Insert a cache entry at the head of the linked list
 */
void dllist_insert_head(struct cache *cache, struct cache_entry *ce)
{
    // Insert at the head of the list
    if (cache->head == NULL) {
        cache->head = cache->tail = ce;
        ce->prev = ce->next = NULL;
    } else {
        cache->head->prev = ce;
        ce->next = cache->head;
        ce->prev = NULL;
        cache->head = ce;
    }
}

/**
 * Move a cache entry to the head of the list
 */
void dllist_move_to_head(struct cache *cache, struct cache_entry *ce)
{
    if (ce != cache->head) {
        if (ce == cache->tail) {
            // We're the tail
            cache->tail = ce->prev;
            cache->tail->next = NULL;

        } else {
            // We're neither the head nor the tail
            ce->prev->next = ce->next;
            ce->next->prev = ce->prev;
        }

        ce->next = cache->head;
        cache->head->prev = ce;
        ce->prev = NULL;
        cache->head = ce;
    }
}


/**
 * Removes the tail from the list and returns it
 * 
 * NOTE: does not deallocate the tail
 */
struct cache_entry *dllist_remove_tail(struct cache *cache)
{
    struct cache_entry *oldtail = cache->tail;

    cache->tail = oldtail->prev;
    cache->tail->next = NULL;

    return oldtail;
}

/**
 * Create a new cache
 * 
 * max_size: maximum number of entries in the cache
 * hashsize: hashtable size (0 for default)
 */
struct cache *cache_create(int max_size, int hashsize)
{
    ///////////////////
    // IMPLEMENT ME! //
    ///////////////////
    struct cache *ret = calloc(1, sizeof(struct cache));
    ret->index = hashtable_create(hashsize, NULL);
    ret->head = NULL;
    ret->tail = NULL;
    ret->max_size = max_size;
    ret->cur_size = 0;
    return ret;
}

void cache_free(struct cache *cache)
{
    struct cache_entry *cur_entry = cache->head;

    hashtable_destroy(cache->index);

    while (cur_entry != NULL) {
        struct cache_entry *next_entry = cur_entry->next;

        free_entry(cur_entry);

        cur_entry = next_entry;
    }

    free(cache);
}

/**
 * Store an entry in the cache
 *
 * This will also remove the least-recently-used items as necessary.
 * 
 * NOTE: doesn't check for duplicate cache entries
 */
void cache_put(struct cache *cache, char *path, char *content_type, void *content, int content_length)
{
    struct cache_entry *ce = alloc_entry(path, content_type, content, content_length);
    // If cache is filled
    if (cache->cur_size == cache->max_size) {
        // Evict the least recently used cache entry both in ddl and ht
        hashtable_delete(cache->index, dllist_remove_tail(cache)->path);
        cache->cur_size--;
    }     
    // Insert and mark it recently used both in ddl and ht
    dllist_insert_head(cache, ce);
    cache->cur_size++;
    hashtable_put(cache->index, ce->path, (void *)ce); 
}

/**
 * Retrieve an entry from the cache
 */
struct cache_entry *cache_get(struct cache *cache, char *path)
{
    // Get cache entry by searching ht
    struct cache_entry *ce = hashtable_get(cache->index, path);

    // Cache entry is not exist
    if (ce == NULL) return NULL;

    // Mark recently used
    dllist_move_to_head(cache, ce);

    return ce;
}
