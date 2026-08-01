#ifndef NINECRAFT_GAME_PARAMETERS_H
#define NINECRAFT_GAME_PARAMETERS_H

#include <stdbool.h>

typedef struct {
    char *home_path;
    char *game_path;
    bool debug_logging;
} game_parameters_t;

extern game_parameters_t game_parameters;

bool game_parameters_debug_requested(int argc, char **argv);
void parse_game_parameters(int argc, char **argv);

#endif
