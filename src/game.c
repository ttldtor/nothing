#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <assert.h>
#include <stdio.h>

#include "game.h"
#include "game/level.h"
#include "game/sound_samples.h"
#include "system/error.h"
#include "system/lt.h"

typedef enum game_state_t {
    GAME_STATE_RUNNING = 0,
    GAME_STATE_PAUSE,
    GAME_STATE_QUIT,

    GAME_STATE_N
} game_state_t;

typedef struct game_t {
    lt_t *lt;

    game_state_t state;
    level_t *level;
    char *level_file_path;
    sound_samples_t *sound_samples;
    camera_t *camera;
} game_t;

game_t *create_game(const char *level_file_path,
                    const char *sound_sample_files[],
                    size_t sound_sample_files_count,
                    SDL_Renderer *renderer)
{
    assert(level_file_path);

    lt_t *const lt = create_lt();
    if (lt == NULL) {
        return NULL;
    }

    game_t *game = PUSH_LT(lt, malloc(sizeof(game_t)), free);
    if (game == NULL) {
        throw_error(ERROR_TYPE_LIBC);
        RETURN_LT(lt, NULL);
    }

    game->level = PUSH_LT(
        lt,
        create_level_from_file(level_file_path),
        destroy_level);
    if (game->level == NULL) {
        RETURN_LT(lt, NULL);
    }

    game->level_file_path = PUSH_LT(lt, malloc(sizeof(char) * (strlen(level_file_path) + 1)), free);
    if (game->level_file_path == NULL) {
        throw_error(ERROR_TYPE_LIBC);
        RETURN_LT(lt, NULL);
    }
    strcpy(game->level_file_path, level_file_path);

    game->camera = PUSH_LT(lt, create_camera_from_renderer(renderer), destroy_camera);
    if (game->camera == NULL) {
        RETURN_LT(lt, NULL);
    }

    game->sound_samples = PUSH_LT(
        lt,
        create_sound_samples(
            sound_sample_files,
            sound_sample_files_count),
        destroy_sound_samples);
    if (game->sound_samples == NULL) {
        RETURN_LT(lt, NULL);
    }

    game->state = GAME_STATE_RUNNING;
    game->lt = lt;

    return game;
}

void destroy_game(game_t *game)
{
    assert(game);
    RETURN_LT0(game->lt);
}

int game_render(const game_t *game)
{
    assert(game);

    if (game->state == GAME_STATE_QUIT) {
        return 0;
    }

    if (level_render(game->level, game->camera) < 0) {
        return -1;
    }

    camera_present(game->camera);

    return 0;
}

int game_sound(game_t *game)
{
    return level_sound(game->level, game->sound_samples);
}

int game_update(game_t *game, float delta_time)
{
    assert(game);
    assert(delta_time > 0.0f);

    if (game->state == GAME_STATE_QUIT) {
        return 0;
    }

    if (game->state == GAME_STATE_RUNNING) {
        return level_update(game->level, delta_time);
    }

    return 0;
}


static int game_event_pause(game_t *game, const SDL_Event *event)
{
    assert(game);
    assert(event);

    switch (event->type) {
    case SDL_QUIT:
        game->state = GAME_STATE_QUIT;
        break;

    case SDL_KEYDOWN:
        switch (event->key.keysym.sym) {
        case SDLK_p:
            game->state = GAME_STATE_RUNNING;
            camera_toggle_blackwhite_mode(game->camera);
            sound_samples_toggle_pause(game->sound_samples);
            break;
        case SDLK_l:
            camera_toggle_debug_mode(game->camera);
            level_toggle_debug_mode(game->level);
            break;
        }
        break;
    }

    return level_event(game->level, event);
}

static int game_event_running(game_t *game, const SDL_Event *event)
{
    assert(game);
    assert(event);

    switch (event->type) {
    case SDL_QUIT:
        game->state = GAME_STATE_QUIT;
        break;

    case SDL_KEYDOWN:
        switch (event->key.keysym.sym) {
        case SDLK_r:
            printf("Reloading the level from '%s'...\n", game->level_file_path);

            game->level = RESET_LT(
                game->lt,
                game->level,
                create_level_from_file(
                    game->level_file_path));

            if (game->level == NULL) {
                print_current_error_msg("Could not reload the level");
                game->state = GAME_STATE_QUIT;
                return -1;
            }
            break;

        case SDLK_q:
            printf("Reloading the level's platforms from '%s'...\n", game->level_file_path);
            if (level_reload_preserve_player(game->level, game->level_file_path) < 0) {
                print_current_error_msg("Could not reload the level");
                game->state = GAME_STATE_QUIT;
                return -1;
            }
            break;

        case SDLK_p:
            game->state = GAME_STATE_PAUSE;
            camera_toggle_blackwhite_mode(game->camera);
            sound_samples_toggle_pause(game->sound_samples);
            break;

        case SDLK_l:
            camera_toggle_debug_mode(game->camera);
            level_toggle_debug_mode(game->level);
            break;
        }
        break;

    }

    return level_event(game->level, event);
}

int game_event(game_t *game, const SDL_Event *event)
{
    assert(game);
    assert(event);

    switch (game->state) {
    case GAME_STATE_RUNNING:
        return game_event_running(game, event);

    case GAME_STATE_PAUSE:
        return game_event_pause(game, event);

    default: {}
    }

    return 0;
}


int game_input(game_t *game,
               const Uint8 *const keyboard_state,
               SDL_Joystick *the_stick_of_joy)
{
    assert(game);
    assert(keyboard_state);

    if (game->state == GAME_STATE_QUIT || game->state == GAME_STATE_PAUSE) {
        return 0;
    }

    return level_input(game->level, keyboard_state, the_stick_of_joy);
}

int game_over_check(const game_t *game)
{
    return game->state == GAME_STATE_QUIT;
}
