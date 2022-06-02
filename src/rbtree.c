#include <kmalloc.h>
#include <rbtree.h>
#include <cpage.h>

//This red-black tree code was made by Dr. Stephen Marz in order 
//to implement the Completely Fair Scheduling algorithm

static RBNode *rb_remove_key_node(RBNode *node, rb_key_t key);
static inline int is_red(const RBNode *node)
{
    if (node) {
        return node->color == RB_TREE_COLOR_RED;
    }
    return 0;
}

static inline int key_compare(rb_key_t key1, rb_key_t key2)
{
    if (key1 > key2) {
        return 1;
    }
    else if (key1 < key2) {
        return -1;
    }
    return 0;
}

static void flip_color(RBNode *node)
{
    node->color        = !node->color;
    node->left->color  = !node->left->color;
    node->right->color = !node->right->color;
}

static RBNode *rotate_left(RBNode *left)
{
    RBNode *right;

    if (!left) {
        return NULL;
    }
    right        = left->right;
    left->right  = right->left;
    right->left  = left;
    right->color = left->color;
    left->color  = RB_TREE_COLOR_RED;
    return right;
}

static RBNode *rotate_right(RBNode *right)
{
    RBNode *left;

    if (!right) {
        return NULL;
    }
    left         = right->left;
    right->left  = left->right;
    left->right  = right;
    left->color  = right->color;
    right->color = RB_TREE_COLOR_RED;
    return left;
}

static RBNode *create_node(rb_key_t key, rb_value_t value)
{
    RBNode *n;

    //if ((n = (RBNode *)kmalloc(sizeof(*n))) == NULL) {
    //    return NULL;
    //}
    if((n = (RBNode *)cpage_znalloc(1)) == NULL){
        return NULL;
    }
    n->key   = key;
    n->value = value;
    n->color = RB_TREE_COLOR_RED;
    n->left  = NULL;
    n->right = NULL;
    return n;
}

static RBNode *insert_this(RBNode *node, rb_key_t key, rb_value_t value)
{
    int res;

    if (!node) {
        return create_node(key, value);
    }
    res = key_compare(key, node->key);
    if (res == 0) {
        node->value = value;
    }
    else if (res < 0) {
        node->left = insert_this(node->left, key, value);
    }
    else {
        node->right = insert_this(node->right, key, value);
    }

    if (is_red(node->right) && !is_red(node->left)) {
        node = rotate_left(node);
    }
    if (is_red(node->left) && is_red(node->left->left)) {
        node = rotate_right(node);
    }
    if (is_red(node->left) && is_red(node->right)) {
        flip_color(node);
    }

    return node;
}

static RBNode *min(RBNode *node)
{
    if (!node) {
        return NULL;
    }

    while (node->left) {
        node = node->left;
    }

    return node;
}

static inline RBNode *rb_balance(RBNode *node)
{
    if (is_red(node->right)) {
        node = rotate_left(node);
    }
    if (is_red(node->left) && is_red(node->left->left)) {
        node = rotate_right(node);
    }
    if (is_red(node->left) && is_red(node->right)) {
        flip_color(node);
    }

    return node;
}

static RBNode *move_red_to_left(RBNode *node)
{
    flip_color(node);

    if (node && node->right && is_red(node->right->left)) {
        node->right = rotate_right(node->right);
        node        = rotate_left(node);
        flip_color(node);
    }

    return node;
}

static RBNode *move_red_to_right(RBNode *node)
{
    flip_color(node);

    if (node && node->left && is_red(node->left->left)) {
        node = rotate_right(node);
        flip_color(node);
    }

    return node;
}

static RBNode *remove_min(RBNode *node)
{
    if (!node) {
        return NULL;
    }
    if (!node->left) {
        kfree(node);
        return NULL;
    }
    if (!is_red(node->left) && !is_red(node->left->left)) {
        node = move_red_to_left(node);
    }

    node->left = remove_min(node->left);

    return rb_balance(node);
}

static RBNode *remove_it(RBNode *node, rb_key_t key)
{
    RBNode *tmp;

    if (!node) {
        return NULL;
    }
    if (key_compare(key, node->key) == -1) {
        if (node->left) {
            if (!is_red(node->left) && !is_red(node->left->left)) {
                node = move_red_to_left(node);
            }
            node->left = rb_remove_key_node(node->left, key);
        }
    }
    else {
        if (is_red(node->left)) {
            node = rotate_right(node);
        }
        if (!key_compare(key, node->key) && !node->right) {
            kfree(node);
            return NULL;
        }
        if (node->right) {
            if (!is_red(node->right) && !is_red(node->right->left)) {
                node = move_red_to_right(node);
            }
            if (!key_compare(key, node->key)) {
                tmp         = min(node->right);
                node->key   = tmp->key;
                node->value = tmp->value;
                node->right = remove_min(node->right);
            }
            else
                node->right = rb_remove_key_node(node->right, key);
        }
    }

    return rb_balance(node);
}

static RBNode *rb_remove_key_node(RBNode *node, rb_key_t key)
{
    node = remove_it(node, key);

    if (node) {
        node->color = RB_TREE_COLOR_BLACK;
    }

    return node;
}

static void rb_erase_node(RBNode *node)
{
    if (node) {
        if (node->left) {
            rb_erase_node(node->left);
        }
        if (node->right) {
            rb_erase_node(node->right);
        }
        node->left  = NULL;
        node->right = NULL;
        kfree(node);
    }
}

/////////////////////////
// FUNCTIONS
/////////////////////////

RBTree *rb_new(void)
{
    //return (RBTree*)kzalloc(sizeof(RBTree));
    return (RBTree*)cpage_znalloc(100);
}

void rb_insert(RBTree *tree, rb_key_t key, rb_value_t value)
{
    if (!tree) {
        return;
    }
    tree->root = insert_this(tree->root, key, value);
    if (tree->root) {
        tree->root->color = RB_TREE_COLOR_BLACK;
    }
}

bool rb_find(const RBTree *tree, rb_key_t key, rb_value_t *value)
{
    const RBNode *node;
    const RBNode *next;
    int cmp;

    if (!tree) {
        return false;
    }
    for (node = tree->root; node; node = next) {
        if (!(cmp = key_compare(key, node->key))) {
            *value = node->value;
            return true;
        }
        if (cmp < 0) {
            next = node->left;
        }
        else {
            next = node->right;
        }
    }

    return false;
}

void rb_delete(RBTree *tree, rb_key_t key)
{
    if (!tree) {
        return;
    }

    tree->root = rb_remove_key_node(tree->root, key);
}

void rb_free(RBTree *tree)
{
    if (tree) {
        rb_erase_node(tree->root);
        tree->root = NULL;
    }
}