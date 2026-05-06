#include "world_seed.h"

#include <stdlib.h>
#include <time.h>

unsigned int world_random_seed(void) {
    return (unsigned int)time(NULL) ^ (unsigned int)clock();
}

void world_seed_rng(unsigned int seed) {
    srand(seed);
}
