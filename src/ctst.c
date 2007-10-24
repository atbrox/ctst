/*
  ctst - (c) 2007 Nicolas Lehuen
  This is released under the LGPL - see http://www.gnu.org/licenses/lgpl.html

  $Id: $

  This file contains the implementation of the ctst.
*/
#include "include/ctst.h"

struct struct_ctst_ctst {
  ctst_storage* storage;
  ctst_node_ref root;
  size_t size;
  size_t total_key_length;
};

typedef struct struct_ctst_balance_info {
  ctst_node_ref node;
  int did_balance;
  int height;
  int balance;
  int left_balance;
  int right_balance;
} ctst_balance_info;

/* Private functions of this module */
ctst_balance_info _ctst_recursive_set(ctst_ctst* ctst, char* bytes, size_t bytes_index, size_t bytes_length,ctst_data data,ctst_node_ref node, size_t local_index);
ctst_node_ref _ctst_new_node(ctst_ctst* ctst, char* bytes, size_t bytes_index, size_t bytes_length,ctst_data data, size_t local_index);

/* ctst allocation / deallocation */

ctst_ctst* ctst_alloc(ctst_storage* storage) {
  ctst_ctst* ctst = (ctst_ctst*)malloc(sizeof(ctst_ctst));
  ctst->storage = storage;
  ctst->root = 0;
  ctst->size = 0;
  ctst->total_key_length = 0;
  return ctst;
}

void ctst_free(ctst_ctst* ctst) {
  free(ctst);
}

/* A few statistics about the ctst */

size_t ctst_get_size(ctst_ctst* ctst) {
  return ctst->size;
}

size_t ctst_get_total_key_length(ctst_ctst* ctst) {
  return ctst->total_key_length;
}

/* Basic accessors : get and set */

ctst_data ctst_get(ctst_ctst* ctst, char* bytes, size_t bytes_index, size_t bytes_length) {
  ctst_node_ref node = ctst->root;
  size_t index = 0;
  
  while(node!=0) {
    /* We load the bytes from the node into local memory */
    char* node_bytes = ctst_storage_load_bytes(ctst->storage,node);
    size_t node_bytes_length = ctst_storage_get_bytes_length(ctst->storage,node);

    size_t local_index=0;
    int diff=0;
    
    /* We keep advancing within the node while the bytes match */
    while (local_index < node_bytes_length && index < bytes_length) {
      diff = bytes[bytes_index] - node_bytes[local_index];
      if (diff == 0) {
          local_index++;
          bytes_index++;
          index++;
      } else {
          break;
      }
    } 

    /* We unload the bytes from local memory */
    ctst_storage_unload_bytes(ctst->storage,node,node_bytes);
    
    if (diff != 0) {
      /* We got a mismatch. */

      if (local_index < node_bytes_length - 1) {
        /* We stopped before the last byte of the node.
           A match is impossible, otherwise a split
           would have occured during the insertion. */
          return 0;
      } else if (diff > 0) {
        /* Mismatch on the last byte, we go to the left */
        node = ctst_storage_get_left(ctst->storage,node);
      } else {
        /* Mismatch on the last byte, we go to the left */
        node = ctst_storage_get_right(ctst->storage,node);
      }
    } else if (local_index == node_bytes_length) {
      /* We matched all the bytes of the node */

      if (index == bytes_length) {
        /* We also matched all the bytes of the key, so
           we've got our result ! */
        return ctst_storage_get_data(ctst->storage,node);
      } else {
        /* The key is not over yet, so we try to advance
           within the tree. */
        node = ctst_storage_get_next(ctst->storage,node);
      }
    } else {
      /* We've got a match but the key ended before the node.
         Therefore we don't have any result (otherwise we would have splitted
         the node during the insertion. */
      return 0;
    }
  }
  
  /* We reached an empty branch, therefore we don't have any result */
  return 0;
}

void ctst_set(ctst_ctst* ctst, char* bytes, size_t bytes_index, size_t bytes_length,ctst_data data) {
  ctst->root = _ctst_recursive_set(ctst,bytes,bytes_index,bytes_length,data,ctst->root,0).node;
  ctst->size++;
  ctst->total_key_length+=bytes_length;
}

ctst_balance_info _ctst_recursive_set(ctst_ctst* ctst, char* bytes, size_t bytes_index, size_t bytes_length,ctst_data data,ctst_node_ref node, size_t local_index) {
  if(node==0) {
    ctst_balance_info balance_info;
    balance_info.node=_ctst_new_node(ctst,bytes,bytes_index,bytes_length,data,local_index);
    balance_info.did_balance=0;
    balance_info.height=1;
    balance_info.balance=0;
    balance_info.left_balance=0;
    balance_info.right_balance=0;
    return balance_info;
  }
  else {
    ctst_balance_info balance_info;

    /* We load the bytes from the node into local memory */
    char* node_bytes = ctst_storage_load_bytes(ctst->storage,node);
    size_t node_bytes_length = ctst_storage_get_bytes_length(ctst->storage,node);

    size_t node_index = 0;
    int diff = 0;
    
    /* We keep advancing within the node while the bytes match */
    while (node_index < node_bytes_length && local_index < bytes_length) {
      diff = bytes[bytes_index+local_index] - node_bytes[node_index];
      if (diff == 0) {
          local_index++;
          node_index++;
      } else {
          break;
      }
    } 

    if(diff!=0) {
      if(node_index<node_bytes_length-1) {
        /* Since there is a mismatch before the last byte of the node, we
           need to split the node */
        ctst_two_node_refs splitted = ctst_storage_split_node(ctst->storage,node,node_index);
        /* TODO : maybe the second node could be joined with its next */
        node = splitted.ref1;
      }
      
      /* TODO : continue here */
    }

    return balance_info;
  }
}

ctst_node_ref _ctst_new_node(ctst_ctst* ctst, char* bytes, size_t bytes_index, size_t bytes_length,ctst_data data, size_t local_index) {
  size_t local_size = bytes_length - local_index;
  if(local_size>ctst_max_bytes_per_node) {
    ctst_node_ref next;

  local_size = ctst_max_bytes_per_node;
  next = _ctst_new_node(ctst,bytes,bytes_index,bytes_length,data,local_index+local_size);
    return ctst_storage_node_alloc(ctst->storage,0,next,0,0,bytes,bytes_index+local_index,local_size);    
  }
  else {
    return ctst_storage_node_alloc(ctst->storage,data,0,0,0,bytes,bytes_index+local_index,local_size);
  }
}
