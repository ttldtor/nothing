#include "boxes.h"
#include "platforms.h"
#include "player.h"
#include "player/rigid_rect.h"
#include "solid.h"

void solid_touches_rect_sides(solid_ref_t solid,
                              rect_t object,
                              int sides[RECT_SIDE_N])
{
    switch (solid.tag) {
    case SOLID_PLATFORMS:
        platforms_touches_rect_sides((platforms_t *) solid.ptr, object, sides);
        break;

    case SOLID_BOXES:
        boxes_touches_rect_sides((boxes_t *) solid.ptr, object, sides);
        break;

    case SOLID_RIGID_RECT:
        rigid_rect_touches_rect_sides((rigid_rect_t *) solid.ptr, object, sides);
        break;

    case SOLID_PLAYER:
        player_touches_rect_sides((player_t *) solid.ptr, object, sides);
        break;
    }
}
