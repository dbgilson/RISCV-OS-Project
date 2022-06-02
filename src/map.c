#include <kmalloc.h>
#include <map.h>
#include <printf.h>
#include <stddef.h>

Map *map_init(Map *mp)
{
    mp->head.next = &mp->head;
    mp->head.prev = &mp->head;

    return mp;
}

Map *map_new(void)
{
    Map *m = (Map *)kmalloc(sizeof(Map));
    if (m == NULL) {
        return NULL;
    }
    return map_init(m);
}

void map_add(Map *mp, uint64_t key, void *value)
{
    MapElem *e;
    MapElem *l = map_find_elem(mp, key);
    if (l == NULL) {
        l = (MapElem *)kzalloc(sizeof(MapElem));
        map_for_each_descending(mp, e);
        l->next       = e->next;
        l->prev       = e;
        l->next->prev = l;
        l->prev->next = l;
    }
    l->key   = key;
    l->value = value;
}

void map_clear(Map *mp)
{
    MapElem *e, *n;
    for (e = mp->head.next; e != &mp->head; e = n) {
        n = e->next;
        map_remove_elem(e);
    }
}

void *map_remove(Map *mp, uint64_t key)
{
    MapElem *e;
    void *ret = NULL;
    map_for_each(mp, e)
    {
        if (e->key == key) {
            ret = e->value;
            map_remove_elem(e);
            break;
        }
    }
    return ret;
}

void *map_remove_elem(MapElem *e)
{
    void *ret     = e->value;
    e->next->prev = e->prev;
    e->prev->next = e->next;
    kfree(e);
    return ret;
}

void *map_remove_by_value(Map *mp, void *value)
{
    MapElem *e;
    void *ret = NULL;
    map_for_each(mp, e)
    {
        if (e->value == value) {
            ret = e->value;
            map_remove_elem(e);
            break;
        }
    }
    return ret;
}

MapElem *map_find_elem(Map *mp, uint64_t key)
{
    MapElem *e;
    map_for_each(mp, e)
    {
        if (e->key == key) {
            return e;
        }
    }
    return NULL;
}

void *map_find(Map *mp, uint64_t key)
{
    MapElem *e;
    e = map_find_elem(mp, key);
    if (e == NULL) {
        return NULL;
    }
    return e->value;
}

void map_free(Map *mp)
{
    MapElem *e, *n;
    for (e = mp->head.next; e != &mp->head; e = n) {
        n = e->next;
        kfree(e);
    }
    kfree(mp);
}

void map_print(const Map *mp)
{
    int i = 0;
    MapElem *e;
    map_for_each_ascending(mp, e)
    {
        printf("[%d] %lu -> 0x%08lx\n", i++, e->key, e->value);
    }
    if (i == 0) {
        printf("Map is empty\n");
    }
}
