#ifndef GOALS_H_
#define GOALS_H_

#include <SDL2/SDL.h>

#include "game/camera.h"
#include "game/level/player.h"
#include "game/sound_samples.h"

typedef struct goals_t goals_t;

goals_t *create_goals_from_stream(FILE *stream);
void destroy_goals(goals_t *goals);

rect_t goals_hitbox(const goals_t *goals);

int goals_render(const goals_t *goals,
                 const camera_t *camera);
int goals_sound(goals_t *goals,
                sound_samples_t *sound_samples);
void goals_update(goals_t *goals,
                  float delta_time);
void goals_hide(goals_t *goals,
                rect_t player_hitbox);
void goals_checkpoint(const goals_t *goals,
                      player_t *player);
void goals_cue(goals_t *goals,
               const camera_t *camera);

#endif  // GOALS_H_
