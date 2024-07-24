#include "Hashset.h"
static bool DUMMY_POINTER_LOCATION = false;
const uint64_t HASHSET_TOMBSTONE = (uint64_t)&DUMMY_POINTER_LOCATION;