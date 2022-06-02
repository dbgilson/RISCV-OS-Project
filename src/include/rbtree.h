#pragma once

#include <stdbool.h>
#include <stddef.h>

//This red-black tree code was made by Dr. Stephen Marz in order 
//to implement the Completely Fair Scheduling algorithm

#define RB_TREE_COLOR_BLACK 0
#define RB_TREE_COLOR_RED   1

typedef unsigned long rb_key_t;
typedef unsigned long rb_value_t;

typedef struct RBNode {
    rb_key_t key;
    rb_value_t value;
    int color;
    struct RBNode *left;
    struct RBNode *right;
} RBNode;

typedef struct RBTree {
    RBNode *root;
} RBTree;

RBTree *rb_new(void);
void rb_free(RBTree *tree);
void rb_delete(RBTree *tree, rb_key_t key);
bool rb_find(const RBTree *tree, rb_key_t key, rb_value_t *value);
void rb_insert(RBTree *tree, rb_key_t key, rb_value_t value);
