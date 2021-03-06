#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "game.h"
#include "game/level/platforms.h"
#include "game/level/player.h"
#include "game/sound_samples.h"
#include "math/minmax.h"
#include "math/point.h"
#include "system/error.h"
#include "system/lt.h"

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600

/* LT module adapter for Mix_CloseAudio */
static void Mix_CloseAudio_lt(void* ignored)
{
    (void) ignored;
    Mix_CloseAudio();
}

/* LT module adapter for SDL_Quit */
static void SDL_Quit_lt(void* ignored)
{
    (void) ignored;
    SDL_Quit();
}

static void print_usage(FILE *stream)
{
    fprintf(stream, "Usage: nothing [--fps <fps>] <level-file>\n");
}

int main(int argc, char *argv[])
{
    srand((unsigned int) time(NULL));

    lt_t *const lt = create_lt();

    char *level_filename = NULL;
    int fps = 60;

    for (int i = 1; i < argc;) {
        if (strcmp(argv[i], "--fps") == 0) {
            if (i + 1 < argc) {
                if (sscanf(argv[i + 1], "%d", &fps) == 0) {
                    fprintf(stderr, "Cannot parse FPS: %s is not a number\n", argv[i + 1]);
                    print_usage(stderr);
                    RETURN_LT(lt, -1);
                }
                i += 2;
            } else {
                fprintf(stderr, "Value of FPS is not provided\n");
                print_usage(stderr);
                RETURN_LT(lt, -1);
            }
        } else {
            level_filename = argv[i];
            i++;
        }
    }

    if (level_filename == NULL) {
        fprintf(stderr, "Path to level file is not provided\n");
        print_usage(stderr);
        RETURN_LT(lt, -1);
    }

    if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
        print_error_msg(ERROR_TYPE_SDL2, "Could not initialize SDL");
        RETURN_LT(lt, -1);
    }
    PUSH_LT(lt, 42, SDL_Quit_lt);

    SDL_ShowCursor(SDL_DISABLE);

    SDL_Window *const window = PUSH_LT(
        lt,
        SDL_CreateWindow(
            "Nothing",
            100, 100,
            SCREEN_WIDTH, SCREEN_HEIGHT,
            SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE),
        SDL_DestroyWindow);

    if (window == NULL) {
        print_error_msg(ERROR_TYPE_SDL2, "Could not create SDL window");
        RETURN_LT(lt, -1);
    }

    SDL_Renderer *const renderer = PUSH_LT(
        lt,
        SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC),
        SDL_DestroyRenderer);
    if (renderer == NULL) {
        print_error_msg(ERROR_TYPE_SDL2, "Could not create SDL renderer");
        RETURN_LT(lt, -1);
    }
    if (SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND) < 0) {
        print_error_msg(ERROR_TYPE_SDL2, "Could not set up blending mode for the renderer");
        RETURN_LT(lt, -1);
    }

    SDL_Joystick *the_stick_of_joy = NULL;

    if (SDL_NumJoysticks() > 0) {
        the_stick_of_joy = PUSH_LT(lt, SDL_JoystickOpen(0), SDL_JoystickClose);

        if (the_stick_of_joy == NULL) {
            print_error_msg(ERROR_TYPE_SDL2, "Could not open 0th Stick of the Joy: %s\n");
            RETURN_LT(lt, -1);
        }

        printf("Opened Joystick 0\n");
        printf("Name: %s\n", SDL_JoystickNameForIndex(0));
        printf("Number of Axes: %d\n", SDL_JoystickNumAxes(the_stick_of_joy));
        printf("Number of Buttons: %d\n", SDL_JoystickNumButtons(the_stick_of_joy));
        printf("Number of Balls: %d\n", SDL_JoystickNumBalls(the_stick_of_joy));

        SDL_JoystickEventState(SDL_ENABLE);
    } else {
        fprintf(stderr, "[WARNING] Could not find any Sticks of the Joy\n");
    }

    if (Mix_OpenAudio(
            MIX_DEFAULT_FREQUENCY,
            MIX_DEFAULT_FORMAT,
            2,
            1024) < 0) {
        print_error_msg(ERROR_TYPE_SDL2_MIXER, "Could not initialize the audio\n");
        RETURN_LT(lt, -1);
    }
    PUSH_LT(lt, 42, Mix_CloseAudio_lt);

    // ------------------------------

    const char * sound_sample_files[] = {
        "./sounds/nothing.wav",
        "./sounds/something.wav"
    };
    const size_t sound_sample_files_count = sizeof(sound_sample_files) / sizeof(char*);

    game_t *const game = PUSH_LT(
        lt,
        create_game(
            level_filename,
            sound_sample_files,
            sound_sample_files_count,
            renderer),
        destroy_game);
    if (game == NULL) {
        print_current_error_msg("Could not create the game object");
        RETURN_LT(lt, -1);
    }

    const Uint8 *const keyboard_state = SDL_GetKeyboardState(NULL);

    SDL_Event e;
    int64_t last_time = (int64_t) SDL_GetTicks();
    const int64_t expected_delay_ms = (int64_t) (1000.0f / (float) fps);
    while (!game_over_check(game)) {
        const int64_t current_time = (int64_t) SDL_GetTicks();

        if (current_time == last_time) {
            SDL_Delay((Uint32) expected_delay_ms);
            last_time = current_time;
            continue;
        }

        const int64_t actual_delta_ms = current_time - last_time;


        while (!game_over_check(game) && SDL_PollEvent(&e)) {
            if (game_event(game, &e) < 0) {
                print_current_error_msg("Failed handling event");
                RETURN_LT(lt, -1);
            }
        }

        if (game_input(game, keyboard_state, the_stick_of_joy) < 0) {
            print_current_error_msg("Failed handling input");
            RETURN_LT(lt, -1);
        }

        if (game_render(game) < 0) {
            print_current_error_msg("Failed rendering the game");
            RETURN_LT(lt, -1);
        }

        int64_t effective_delta_ms = max_int64(actual_delta_ms, expected_delay_ms);
        while (effective_delta_ms > 0) {
            if (game_update(game, (float) min_int64(expected_delay_ms, effective_delta_ms) * 0.001f) < 0) {
                print_current_error_msg("Failed handling updating");
                RETURN_LT(lt, -1);
            }

            effective_delta_ms -= expected_delay_ms;
        }

        if (game_sound(game) < 0) {
            print_current_error_msg("Failed handling the sound");
            RETURN_LT(lt, -1);
        }

        SDL_Delay((unsigned int) max_int64(0, expected_delay_ms - actual_delta_ms));
        last_time = current_time;
    }

    RETURN_LT(lt, 0);
}
