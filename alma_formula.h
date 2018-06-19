#ifndef alma_formula_h
#define alma_formula_h

#include "mpc/mpc.h"

typedef enum node_type {FOL, FUNCTION} node_type;
typedef enum alma_operator {NOT, OR, AND, IF} alma_operator;
typedef enum if_tag {FIF, BIF} if_tag;

struct alma_fol;
struct alma_function;

typedef struct alma_node {
  node_type type;
  union {
    struct alma_fol *fol;
    struct alma_function *function;
  } data;
} alma_node;

typedef struct alma_fol {
  alma_operator op; // Which arguments are used is implicit in operator
  alma_node *arg1; // Holds antecedent when op is IF
  alma_node *arg2; // Holds consequent when op is IF
  if_tag *tag;
} alma_fol;

// Temporary way of storing functions in ALMA sentences
// Eventually will change to format more removed from mpc_ast format
typedef struct alma_function {
  mpc_ast_t *contents;
} alma_function;

void alma_function_init(alma_node *node, mpc_ast_t *ast);
void alma_fol_init(alma_fol fol, alma_operator op, alma_node *arg1, alma_node *arg2, if_tag *tag);

void generate_alma_trees(mpc_ast_t *tree, alma_node **alma_trees, size_t *size);

void alma_print(alma_node node);

void free_alma_tree(alma_node *node);

void mpc_ast_delete_selective(mpc_ast_t *a);

#endif
