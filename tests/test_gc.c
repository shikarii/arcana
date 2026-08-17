/*
 * Arcana test suite — GC tests.
 * Split from test_all.c for modularity.
 */
#include "test_harness.h"

TEST(test_gc_collect_unreachable) {
    ArcGC gc; arc_gc_init(&gc);
    /* Allocate objects then make them unreachable */
    arc_obj_string_new(&gc, "garbage1", 8);
    arc_obj_string_new(&gc, "garbage2", 8);
    ASSERT(gc.objects != NULL);
    /* Collect with empty roots — all should be freed */
    arc_gc_collect(&gc, NULL, 0, NULL, 0, NULL);
    ASSERT(gc.objects == NULL);
    arc_gc_free_all(&gc);
}

TEST(test_gc_preserve_reachable) {
    ArcGC gc; arc_gc_init(&gc);
    ArcObjString* s1 = arc_obj_string_new(&gc, "keep", 4);
    arc_obj_string_new(&gc, "discard", 7);
    /* Put s1 on a fake stack */
    ArcValue stack[1] = { arc_val_obj((ArcObject*)s1) };
    arc_gc_collect(&gc, stack, 1, NULL, 0, NULL);
    /* s1 should survive, discard should be freed */
    ASSERT(gc.objects != NULL);
    ASSERT(gc.objects->next == NULL);  /* only one object remains */
    ASSERT(strcmp(((ArcObjString*)gc.objects)->data, "keep") == 0);
    arc_gc_free_all(&gc);
}

TEST(test_gc_array_tracing) {
    ArcGC gc; arc_gc_init(&gc);
    ArcObjArray* arr = arc_obj_array_new(&gc, 4);
    ArcObjString* s = arc_obj_string_new(&gc, "inside", 6);
    arc_obj_array_push(arr, arc_val_obj((ArcObject*)s));
    arc_obj_string_new(&gc, "outside", 7);  /* unreachable */
    ArcValue stack[1] = { arc_val_obj((ArcObject*)arr) };
    arc_gc_collect(&gc, stack, 1, NULL, 0, NULL);
    /* arr and s survive, "outside" is collected */
    ASSERT(arr->count == 1);
    ASSERT(ARC_IS_STRING(arr->items[0]));
    ASSERT(strcmp(ARC_AS_STRING(arr->items[0])->data, "inside") == 0);
    arc_gc_free_all(&gc);
}

TEST(test_gc_map_tracing) {
    ArcGC gc; arc_gc_init(&gc);
    ArcObjMap* map = arc_obj_map_new(&gc, 4);
    ArcObjString* key = arc_obj_string_new(&gc, "k", 1);
    ArcObjString* val = arc_obj_string_new(&gc, "v", 1);
    arc_obj_map_set(map, arc_val_obj((ArcObject*)key), arc_val_obj((ArcObject*)val));
    arc_obj_string_new(&gc, "orphan", 6);
    ArcValue stack[1] = { arc_val_obj((ArcObject*)map) };
    arc_gc_collect(&gc, stack, 1, NULL, 0, NULL);
    /* map, key, val survive; orphan collected */
    ArcValue out;
    ASSERT(arc_obj_map_get(map, arc_val_obj((ArcObject*)key), &out));
    ASSERT(ARC_IS_STRING(out));
    arc_gc_free_all(&gc);
}

TEST(test_gc_stress) {
    ArcGC gc; arc_gc_init(&gc);
    /* Allocate many strings to trigger GC threshold */
    ArcObjString* keeper = arc_obj_string_new(&gc, "root", 4);
    for (int i = 0; i < 500; i++) {
        arc_obj_string_new(&gc, "temp", 4);
    }
    /* Collect with keeper as only root */
    ArcValue stack[1] = { arc_val_obj((ArcObject*)keeper) };
    arc_gc_collect(&gc, stack, 1, NULL, 0, NULL);
    /* Only keeper should remain */
    int count = 0;
    for (ArcObject* o = gc.objects; o; o = o->next) count++;
    ASSERT(count == 1);
    ASSERT(strcmp(keeper->data, "root") == 0);
    arc_gc_free_all(&gc);
}

TEST(test_gc_stress_mode) {
    ArcGC gc; arc_gc_init(&gc);
    arc_gc_set_stress(&gc, true);
    ASSERT(gc.stress_mode == true);

    /* Allocate objects; stress mode flag is set */
    ArcObjString* s1 = arc_obj_string_new(&gc, "one", 3);
    ArcObjString* s2 = arc_obj_string_new(&gc, "two", 3);
    ASSERT(s1 != NULL);
    ASSERT(s2 != NULL);
    ASSERT(gc.alloc_count >= 2);

    /* Manually collect with both as roots */
    ArcValue stack[2] = { arc_val_obj((ArcObject*)s1), arc_val_obj((ArcObject*)s2) };
    arc_gc_collect(&gc, stack, 2, NULL, 0, NULL);
    ASSERT(gc.collect_count == 1);

    /* Both should survive */
    int count = 0;
    for (ArcObject* o = gc.objects; o; o = o->next) count++;
    ASSERT(count == 2);

    arc_gc_free_all(&gc);
}
