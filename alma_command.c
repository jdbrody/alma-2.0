#include <stdlib.h>
#include "alma_command.h"
#include "tommyds/tommyds/tommytypes.h"
#include "tommyds/tommyds/tommyarray.h"
#include "tommyds/tommyds/tommyhashlin.h"
#include "tommyds/tommyds/tommylist.h"
#include "alma_kb.h"
#include "alma_formula.h"
#include "alma_parser.h"
#include "alma_print.h"

// Caller will need to free collection with kb_halt
void kb_init(kb **collection, char *file) {
  // Allocate and initialize
  *collection = malloc(sizeof(**collection));
  kb *collec = *collection;

  collec->time = 1;
  collec->prev_str = NULL;
  collec->now_str = NULL;
  collec->idling = 0;
  tommy_array_init(&collec->new_clauses);
  tommy_list_init(&collec->clauses);
  tommy_hashlin_init(&collec->index_map);
  tommy_hashlin_init(&collec->pos_map);
  tommy_list_init(&collec->pos_list);
  tommy_hashlin_init(&collec->neg_map);
  tommy_list_init(&collec->neg_list);
  tommy_hashlin_init(&collec->fif_map);
  tommy_array_init(&collec->res_task_list);
  collec->res_task_count = 0;
  tommy_hashlin_init(&collec->fif_tasks);
  tommy_hashlin_init(&collec->distrusted);

  parse_init();
  assert_formula(collec, "now(1).", 0);

  // Given a file argument, obtain other initial clauses from
  if (file != NULL) {
    alma_node *trees;
    int num_trees;

    if (formulas_from_source(file, 1, &num_trees, &trees)) {
      nodes_to_clauses(trees, num_trees, &collec->new_clauses, 0);
      fif_to_front(&collec->new_clauses);
    }
    // If file cannot parse, cleanup and exit
    else {
      kb_halt(collec);
      exit(0);
    }
  }

  // Insert starting clauses
  for (tommy_size_t i = 0; i < tommy_array_size(&collec->new_clauses); i++) {
    clause *c = tommy_array_get(&collec->new_clauses, i);
    // Insert into KB if not duplicate
    if (duplicate_check(collec, c) == NULL)
      add_clause(collec, c);
    else
      free_clause(c);
  }
  tommy_array_done(&collec->new_clauses);
  tommy_array_init(&collec->new_clauses);

  // Generate starting resolution tasks
  tommy_node *i = tommy_list_head(&collec->clauses);
  while (i) {
    clause *c = ((index_mapping*)i->data)->value;
    if (c->tag == FIF)
      fif_task_map_init(collec, c);
    else {
      res_tasks_from_clause(collec, c, 0);
      fif_tasks_from_clause(collec, c);
    }
    i = i->next;
  }
}

void kb_step(kb *collection) {
  collection->time++;

  collection->now_str = now(collection->time);
  assert_formula(collection, collection->now_str, 0);
  if (collection->prev_str != NULL) {
    delete_formula(collection, collection->prev_str, 0);
    free(collection->prev_str);
  }
  else
    delete_formula(collection, "now(1).", 0);
  collection->prev_str = collection->now_str;

  process_res_tasks(collection);
  process_fif_tasks(collection);


  fif_to_front(&collection->new_clauses);
  // Insert new clauses derived that are not duplicates
  for (tommy_size_t i = 0; i < tommy_array_size(&collection->new_clauses); i++) {
    clause *c = tommy_array_get(&collection->new_clauses, i);
    clause *dupe = duplicate_check(collection, c);
    if (dupe == NULL) {
      res_tasks_from_clause(collection, c, 1);
      fif_tasks_from_clause(collection, c);

      add_clause(collection, c);

      if (c->parents != NULL) {
        int distrust = 0;
        // Update child info for parents of new clause, check for distrusted parents
        for (int j = 0; j < c->parents[0].count; j++) {
          add_child(c->parents[0].clauses[j], c);
          if (is_distrusted(collection, c->parents[0].clauses[j]->index))
            distrust = 1;
        }
        if (distrust) {
          char *time_str = long_to_str(collection->time);
          distrust_recursive(collection, c, time_str);
          free(time_str);
        }
      }
    }
    else {
      if (c->parents != NULL) {
        // A duplicate clause derivation should be acknowledged by adding extra parents to the clause it duplicates
        // Copy parents of c to dupe
        dupe->parents = realloc(dupe->parents, sizeof(*dupe->parents) * (dupe->parent_set_count + c->parent_set_count));
        for (int j = dupe->parent_set_count, k = 0; j < dupe->parent_set_count + c->parent_set_count; j++, k++) {
          dupe->parents[j].count = c->parents[k].count;
          dupe->parents[j].clauses = malloc(sizeof(*dupe->parents[j].clauses) * dupe->parents[j].count);
          memcpy(dupe->parents[j].clauses, c->parents[k].clauses, sizeof(*dupe->parents[j].clauses) * dupe->parents[j].count);
        }
        //memcpy(dupe->parents + dupe->parent_set_count, c->parents, sizeof(*c->parents) * c->parent_set_count);
        dupe->parent_set_count += c->parent_set_count;

        // Parents of c also gain new child in dupe
        // Also check if parents of c are distrusted
        int distrust = 0;
        for (int j = 0; j < c->parents[0].count; j++) {
          int insert = 1;
          if (is_distrusted(collection, c->parents[0].clauses[j]->index))
            distrust = 1;
          for (int k = 0; k < c->parents[0].clauses[j]->children_count; k++) {
            if (c->parents[0].clauses[j]->children[k] == dupe) {
              insert = 0;
              break;
            }
          }
          if (insert)
            add_child(c->parents[0].clauses[j], dupe);
        }
        if (distrust && !is_distrusted(collection, dupe->index)) {
          char *time_str = long_to_str(collection->time);
          distrust_recursive(collection, dupe, time_str);
          free(time_str);
        }

      }

      free_clause(c);
    }
  }

  // Time always advances; idle when no other clauses are added
  if (tommy_array_size(&collection->new_clauses) <= 1) {
    collection->idling = 1;
  }
  if (collection->idling)
    printf("Idling...\n");

  tommy_array_done(&collection->new_clauses);
  tommy_array_init(&collection->new_clauses);
}

void kb_print(kb *collection) {
  tommy_node *i = tommy_list_head(&collection->clauses);
  while (i) {
    index_mapping *data = i->data;
    printf("%ld: ", data->key);
    clause_print(data->value);
    printf("\n");
    i = i->next;
  }
  printf("\n");
}

void kb_halt(kb *collection) {
  // now_str and prev_str alias at this point, only free one
  free(collection->now_str);

  for (tommy_size_t i = 0; i < tommy_array_size(&collection->new_clauses); i++)
    free_clause(tommy_array_get(&collection->new_clauses, i));
  tommy_array_done(&collection->new_clauses);

  tommy_node *curr = tommy_list_head(&collection->clauses);
  while (curr) {
    index_mapping *data = curr->data;
    curr = curr->next;
    free_clause(data->value);
    free(data);
  }
  tommy_hashlin_done(&collection->index_map);

  tommy_list_foreach(&collection->pos_list, free_predname_mapping);
  tommy_hashlin_done(&collection->pos_map);

  tommy_list_foreach(&collection->neg_list, free_predname_mapping);
  tommy_hashlin_done(&collection->neg_map);

  tommy_hashlin_foreach(&collection->fif_map, free_fif_mapping);
  tommy_hashlin_done(&collection->fif_map);

  // Res task pointers are aliases to those freed from collection->clauses, so only free overall task here
  for (tommy_size_t i = 0; i < tommy_array_size(&collection->res_task_list); i++)
    free(tommy_array_get(&collection->res_task_list, i));
  tommy_array_done(&collection->res_task_list);

  tommy_hashlin_foreach(&collection->fif_tasks, free_fif_task_mapping);
  tommy_hashlin_done(&collection->fif_tasks);

  tommy_hashlin_foreach(&collection->distrusted, free);
  tommy_hashlin_done(&collection->distrusted);

  free(collection);

  parse_cleanup();
}

void kb_assert(kb *collection, char *string) {
  assert_formula(collection, string, 1);
}

void kb_remove(kb *collection, char *string) {
  delete_formula(collection, string, 1);
}
