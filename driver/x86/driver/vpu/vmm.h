//--=========================================================================--
//  This file is linux device driver for VPU.
//-----------------------------------------------------------------------------
//
//       This confidential and proprietary software may be used only
//     as authorized by a licensing agreement from Chips&Media Inc.
//     In the event of publication, the following notice is applicable:
//
//            (C) COPYRIGHT 2006 - 2015  CHIPS&MEDIA INC.
//                      ALL RIGHTS RESERVED
//
//       The entire notice above must be reproduced on all authorized
//       copies.
//
//--=========================================================================--
#include "vmm_type.h"

#ifndef __CNM_VIDEO_MEMORY_MANAGEMENT_H__
#define __CNM_VIDEO_MEMORY_MANAGEMENT_H__

#define VMEM_P_ALLOC(_x)         vmalloc(_x)
#define VMEM_P_FREE(_x)          vfree(_x)

#define VMEM_ASSERT(_exp)        if (!(_exp)) { printk(KERN_INFO "VMEM_ASSERT at %s:%d\n", __FILE__, __LINE__); /*while(1);*/ }
#define VMEM_HEIGHT(_tree)       (_tree == NULL ? -1 : _tree->height)

#define MAX(_a, _b)         (_a >= _b ? _a : _b)

typedef enum {
    LEFT,
    RIGHT
} rotation_dir_t;

typedef struct avl_node_data_struct {
    int     key;
    page_t *page;
} avl_node_data_t;

static avl_node_t*
make_avl_node(
    vmem_key_t key,
    page_t *page
    )
{
    avl_node_t *node = (avl_node_t *)VMEM_P_ALLOC(sizeof(avl_node_t));
    node->key     = key;
    node->page    = page;
    node->height  = 0;
    node->left    = NULL;
    node->right   = NULL;

    return node;
}

static int
get_balance_factor(
    avl_node_t *tree
    )
{
    int factor = 0;
    if (tree) {
        factor = VMEM_HEIGHT(tree->right) - VMEM_HEIGHT(tree->left);
    }

    return factor;
}

/*
 * Left Rotation
 *
 *      A                      B
 *       \                    / \
 *        B         =>       A   C
 *       /  \                 \
 *      D    C                 D
 *
 */
static avl_node_t*
rotation_left(
    avl_node_t *tree
    )
{
    avl_node_t *rchild;
    avl_node_t *lchild;

    if (tree == NULL)
        return NULL;

    rchild = tree->right;
    if (rchild == NULL) {
        return tree;
    }

    lchild = rchild->left;
    rchild->left = tree;
    tree->right = lchild;

    tree->height   = MAX(VMEM_HEIGHT(tree->left), VMEM_HEIGHT(tree->right)) + 1;
    rchild->height = MAX(VMEM_HEIGHT(rchild->left), VMEM_HEIGHT(rchild->right)) + 1;

    return rchild;
}

/*
 * Reft Rotation
 *
 *         A                  B
 *       \                  /  \
 *      B         =>       D    A
 *    /  \                     /
 *   D    C                   C
 *
 */
static avl_node_t
*rotation_right(
    avl_node_t *tree
    )
{
    avl_node_t *rchild;
    avl_node_t *lchild;

    if (tree == NULL)
        return NULL;

    lchild = tree->left;
    if (lchild == NULL)
        return NULL;

    rchild = lchild->right;
    lchild->right = tree;
    tree->left = rchild;

    tree->height   = MAX(VMEM_HEIGHT(tree->left), VMEM_HEIGHT(tree->right)) + 1;
    lchild->height = MAX(VMEM_HEIGHT(lchild->left), VMEM_HEIGHT(lchild->right)) + 1;

    return lchild;
}

static avl_node_t
*do_balance(
    avl_node_t *tree
    )
{
    int bfactor = 0, child_bfactor;       /* balancing factor */

    bfactor = get_balance_factor(tree);

    if (bfactor >= 2) {
        child_bfactor = get_balance_factor(tree->right);
        if (child_bfactor == 1 || child_bfactor == 0) {
            tree = rotation_left(tree);
        } else if (child_bfactor == -1) {
            tree->right = rotation_right(tree->right);
            tree        = rotation_left(tree);
        } else {
            printk(KERN_INFO "invalid balancing factor: %d\n", child_bfactor);
            VMEM_ASSERT(0);
            return NULL;
        }
    } else if (bfactor <= -2) {
        child_bfactor = get_balance_factor(tree->left);
        if (child_bfactor == -1 || child_bfactor == 0) {
            tree = rotation_right(tree);
        } else if (child_bfactor == 1) {
            tree->left = rotation_left(tree->left);
            tree       = rotation_right(tree);
        } else {
            printk(KERN_INFO "invalid balancing factor: %d\n", child_bfactor);
            VMEM_ASSERT(0);
            return NULL;
        }
    }

    return tree;
}
static avl_node_t
*unlink_end_node(
    avl_node_t *tree,
    int dir,
    avl_node_t **found_node
    )
{
    avl_node_t *node;
    *found_node = NULL;

    if (tree == NULL)
        return NULL;

    if (dir == LEFT) {
        if (tree->left == NULL) {
            *found_node = tree;
            return NULL;
        }
    } else {
        if (tree->right == NULL) {
            *found_node = tree;
            return NULL;
        }
    }

    if (dir == LEFT) {
        node = tree->left;
        tree->left = unlink_end_node(tree->left, LEFT, found_node);
        if (tree->left == NULL) {
            tree->left = (*found_node)->right;
            (*found_node)->left  = NULL;
            (*found_node)->right = NULL;
        }
    } else {
        node = tree->right;
        tree->right = unlink_end_node(tree->right, RIGHT, found_node);
        if (tree->right == NULL) {
            tree->right = (*found_node)->left;
            (*found_node)->left  = NULL;
            (*found_node)->right = NULL;
        }
    }

    tree->height = MAX(VMEM_HEIGHT(tree->left), VMEM_HEIGHT(tree->right)) + 1;

    return do_balance(tree);
}

static avl_node_t
*avltree_insert(
    avl_node_t *tree,
    vmem_key_t key,
    page_t *page
    )
{
    if (tree == NULL) {
        tree = make_avl_node(key, page);
    } else {
        if (key >= tree->key) {
            tree->right = avltree_insert(tree->right, key, page);
        } else {
            tree->left  = avltree_insert(tree->left, key, page);
        }
    }

    tree = do_balance(tree);

    tree->height = MAX(VMEM_HEIGHT(tree->left), VMEM_HEIGHT(tree->right)) + 1;

    return tree;
}

static avl_node_t
*do_unlink(
    avl_node_t *tree
    )
{
    avl_node_t *node;
    avl_node_t *end_node;
    node = unlink_end_node(tree->right, LEFT, &end_node);
    if (node) {
        tree->right = node;
    } else {
        node = unlink_end_node(tree->left, RIGHT, &end_node);
        if (node)
            tree->left = node;
    }

    if (node == NULL) {
        node = tree->right ? tree->right : tree->left;
        end_node = node;
    }

    if (end_node) {
        end_node->left  = (tree->left != end_node) ? tree->left : end_node->left;
        end_node->right = (tree->right != end_node) ? tree->right : end_node->right;
        end_node->height = MAX(VMEM_HEIGHT(end_node->left), VMEM_HEIGHT(end_node->right)) + 1;
    }

    tree = end_node;

    return tree;
}

static avl_node_t
*avltree_remove(
    avl_node_t *tree,
    avl_node_t **found_node,
    vmem_key_t key
    )
{
    *found_node = NULL;
    if (tree == NULL) {
        printk(KERN_INFO "failed to find key %d\n", (int)key);
        return NULL;
    }

    if (key == tree->key) {
        *found_node = tree;
        tree = do_unlink(tree);
    } else if (key > tree->key) {
        tree->right = avltree_remove(tree->right, found_node, key);
    } else {
        tree->left  = avltree_remove(tree->left, found_node, key);
    }

    if (tree)
        tree->height = MAX(VMEM_HEIGHT(tree->left), VMEM_HEIGHT(tree->right)) + 1;

    tree = do_balance(tree);

    return tree;
}

static void
avltree_free(
    avl_node_t *tree
    )
{
    if (tree == NULL)
        return;
    if (tree->left == NULL && tree->right == NULL) {
        VMEM_P_FREE(tree);
        return;
    }

    avltree_free(tree->left);
    tree->left = NULL;
    avltree_free(tree->right);
    tree->right = NULL;
    VMEM_P_FREE(tree);
}

static avl_node_t
*remove_approx_value(
    avl_node_t *tree,
    avl_node_t **found,
    vmem_key_t key
    )
{
    *found = NULL;
    if (tree == NULL) {
        return NULL;
    }

    if (key == tree->key) {
        *found = tree;
        tree = do_unlink(tree);
    } else if (key > tree->key) {
        tree->right = remove_approx_value(tree->right, found, key);
    } else {
        tree->left  = remove_approx_value(tree->left, found, key);
        if (*found == NULL) {
            *found = tree;
            tree = do_unlink(tree);
        }
    }
    if (tree)
        tree->height = MAX(VMEM_HEIGHT(tree->left), VMEM_HEIGHT(tree->right)) + 1;

    tree = do_balance(tree);

    return tree;
}

static void
set_blocks_free(
    video_mm_t *mm,
    int pageno,
    int npages
    )
{
    int last_pageno     = pageno + npages - 1;
    int i;
    page_t *page;
    page_t *last_page;

    VMEM_ASSERT(npages);

    if (last_pageno >= mm->num_pages) {
        printk(KERN_INFO "set_blocks_free: invalid last page number: %d\n", last_pageno);
        VMEM_ASSERT(0);
        return;
    }

    for (i = pageno; i <= last_pageno; i++) {
        mm->page_list[i].used         = 0;
        mm->page_list[i].alloc_pages  = 0;
        mm->page_list[i].first_pageno = -1;
    }

    page        = &mm->page_list[pageno];
    page->alloc_pages = npages;
    last_page   = &mm->page_list[last_pageno];
    last_page->first_pageno = pageno;

    mm->free_tree = avltree_insert(mm->free_tree, MAKE_KEY_VPU(npages, pageno), page);
}

static void
set_blocks_alloc(
    video_mm_t *mm,
    int pageno,
    int npages
    )
{
    int last_pageno     = pageno + npages - 1;
    int i;
    page_t *page;
    page_t *last_page;

    if (last_pageno >= mm->num_pages) {
        printk(KERN_INFO "set_blocks_free: invalid last page number: %d\n", last_pageno);
        VMEM_ASSERT(0);
        return;
    }

    for (i = pageno; i <= last_pageno; i++) {
        mm->page_list[i].used         = 1;
        mm->page_list[i].alloc_pages  = 0;
        mm->page_list[i].first_pageno = -1;
    }

    page        = &mm->page_list[pageno];
    page->alloc_pages = npages;

    last_page   = &mm->page_list[last_pageno];
    last_page->first_pageno = pageno;

    mm->alloc_tree = avltree_insert(mm->alloc_tree, MAKE_KEY_VPU(page->addr, 0), page);
}

static int
vmem_init(
    video_mm_t *mm,
    unsigned long addr,
    unsigned long size
    )
{
    int i;

    if (NULL == mm)
        return -1;

    mm->base_addr  = (addr+(VMEM_PAGE_SIZE-1))&~(VMEM_PAGE_SIZE-1);
    mm->mem_size   = size&~VMEM_PAGE_SIZE;
    mm->num_pages  = mm->mem_size/VMEM_PAGE_SIZE;
    mm->free_tree  = NULL;
    mm->alloc_tree = NULL;
    mm->free_page_count = mm->num_pages;
    mm->alloc_page_count = 0;
    mm->page_list  = (page_t *)VMEM_P_ALLOC(mm->num_pages*sizeof(page_t));
    if (mm->page_list == NULL) {
        printk(KERN_ERR "%s:%d failed to kmalloc(%d)\n", __func__, __LINE__, (int)(mm->num_pages*sizeof(page_t)));
        return -1;
    }

    for (i = 0; i < mm->num_pages; i++) {
        mm->page_list[i].pageno       = i;
        mm->page_list[i].addr         = mm->base_addr + (unsigned long)i * (unsigned long)VMEM_PAGE_SIZE;
        mm->page_list[i].alloc_pages  = 0;
        mm->page_list[i].used         = 0;
        mm->page_list[i].first_pageno = -1;
    }

    set_blocks_free(mm, 0, mm->num_pages);

    return 0;
}

static int
vmem_exit(
    video_mm_t *mm
    )
{
    if (mm == NULL) {
        printk(KERN_INFO "vmem_exit: invalid handle\n");
        return -1;
    }

    if (mm->free_tree) {
        avltree_free(mm->free_tree);
    }
    if (mm->alloc_tree) {
        avltree_free(mm->alloc_tree);
    }

    if (mm->page_list) {
    VMEM_P_FREE(mm->page_list);
        mm->page_list = NULL;
    }

    mm->base_addr  = 0;
    mm->mem_size   = 0;
    mm->num_pages  = 0;
    mm->page_list  = NULL;
    mm->free_tree  = NULL;
    mm->alloc_tree = NULL;
    mm->free_page_count = 0;
    mm->alloc_page_count = 0;
    return 0;
}

static unsigned long
vmem_alloc(
    video_mm_t *mm,
    int size,
    unsigned long pid
    )
{
    avl_node_t *node;
    page_t *free_page;
    int         npages, free_size;
    int         alloc_pageno;
    unsigned long  ptr;

    if (mm == NULL) {
        printk(KERN_INFO "vmem_alloc: invalid handle\n");
        return -1;
    }

    if (size <= 0)
        return -1;

    npages = (size + VMEM_PAGE_SIZE - 1)/VMEM_PAGE_SIZE;

    mm->free_tree = remove_approx_value(mm->free_tree, &node, MAKE_KEY_VPU(npages, 0));
    if (node == NULL) {
        return -1;
    }
    free_page = node->page;
    free_size = KEY_TO_VALUE(node->key);

    alloc_pageno = free_page->pageno;
    set_blocks_alloc(mm, alloc_pageno, npages);
    if (npages != free_size) {
        int free_pageno = alloc_pageno + npages;
        set_blocks_free(mm, free_pageno, (free_size-npages));
    }

    VMEM_P_FREE(node);

    ptr = mm->page_list[alloc_pageno].addr;
    mm->alloc_page_count += npages;
    mm->free_page_count  -= npages;

    return ptr;
}

static int
vmem_free(
    video_mm_t *mm,
    unsigned long ptr,
    unsigned long pid
    )
{
    unsigned long addr;
    avl_node_t *found;
    page_t *page;
    int pageno, prev_free_pageno, next_free_pageno;
    int prev_size, next_size;
    int merge_page_no, merge_page_size, free_page_size;

    if (mm == NULL) {
        printk(KERN_INFO "vmem_free: invalid handle\n");
        return -1;
    }

    addr = ptr;

    mm->alloc_tree = avltree_remove(mm->alloc_tree, &found, MAKE_KEY_VPU(addr, 0));
    if (found == NULL) {
        printk(KERN_INFO "vmem_free: 0x%08x not found\n", (int)addr);
        VMEM_ASSERT(0);
        return -1;
    }

    /* find previous free block */
    page = found->page;
    pageno = page->pageno;
    free_page_size = page->alloc_pages;
    prev_free_pageno = pageno-1;
    prev_size = -1;
    if (prev_free_pageno >= 0) {
        if (mm->page_list[prev_free_pageno].used == 0) {
            prev_free_pageno = mm->page_list[prev_free_pageno].first_pageno;
            prev_size = mm->page_list[prev_free_pageno].alloc_pages;
        }
    }

    /* find next free block */
    next_free_pageno = pageno + page->alloc_pages;
    next_free_pageno = (next_free_pageno == mm->num_pages) ? -1 : next_free_pageno;
    next_size = -1;
    if (next_free_pageno >= 0) {
        if (mm->page_list[next_free_pageno].used == 0) {
            next_size = mm->page_list[next_free_pageno].alloc_pages;
        }
    }
    VMEM_P_FREE(found);

    /* merge */
    merge_page_no = page->pageno;
    merge_page_size = page->alloc_pages;
    if (prev_size >= 0) {
        mm->free_tree = avltree_remove(mm->free_tree, &found, MAKE_KEY_VPU(prev_size, prev_free_pageno));
        if (found == NULL) {
            VMEM_ASSERT(0);
            return -1;
        }
        merge_page_no = found->page->pageno;
        merge_page_size += found->page->alloc_pages;
        VMEM_P_FREE(found);
    }
    if (next_size >= 0) {
        mm->free_tree = avltree_remove(mm->free_tree, &found, MAKE_KEY_VPU(next_size, next_free_pageno));
        if (found == NULL) {
            VMEM_ASSERT(0);
            return -1;
        }
        merge_page_size += found->page->alloc_pages;
        VMEM_P_FREE(found);
    }

    page->alloc_pages  = 0;
    page->first_pageno = -1;

    set_blocks_free(mm, merge_page_no, merge_page_size);

    mm->alloc_page_count -= free_page_size;
    mm->free_page_count  += free_page_size;

    return 0;
}

static int
vmem_get_info(
    video_mm_t *mm,
    vmem_info_t *info
    )
{
    if (mm == NULL) {
        printk(KERN_INFO "vmem_get_info: invalid handle\n");
        return -1;
    }

    if (info == NULL) {
        return -1;
    }

    info->total_pages = mm->num_pages;
    info->alloc_pages = mm->alloc_page_count;
    info->free_pages  = mm->free_page_count;
    info->page_size   = VMEM_PAGE_SIZE;

    return 0;
}

#endif /* __CNM_VIDEO_MEMORY_MANAGEMENT_H__ */

