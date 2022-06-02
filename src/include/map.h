#pragma once

#include <stdint.h>

typedef struct MapElem {
    uint64_t key;
    void *value;
    struct MapElem *next;
    struct MapElem *prev;
} MapElem;

typedef struct Map {
    MapElem head;
    // TODO: Add a center element
    // to make insertion / find quicker
} Map;

Map *map_init(Map *map);
Map *map_new(void);
void map_add(Map *map, uint64_t key, void *value);
void *map_remove(Map *map, uint64_t key);
void *map_remove_by_value(Map *map, void *value);
void *map_remove_elem(MapElem *value);
void *map_find(Map *map, uint64_t key);
MapElem *map_find_elem(Map *map, uint64_t key);
void map_clear(Map *map);
void map_free(Map *map);
void map_print(const Map *map);

#define map_for_each_ascending_from(map, elem, start) \
    for ((elem) = (start); (elem) != &(map)->head; (elem) = (elem)->prev)

#define map_for_each_descending_from(map, elem, start) \
    for ((elem) = (start); (elem) != &(map)->head; (elem) = (elem)->next)

#define map_for_each_ascending(map, elem) \
    for ((elem) = (map)->head.prev; (elem) != &(map)->head; (elem) = (elem)->prev)

#define map_for_each_descending(map, elem) \
    for ((elem) = (map)->head.prev; (elem) != &(map)->head; (elem) = (elem)->prev)

#define map_for_each(map, elem) map_for_each_ascending(map, elem)
