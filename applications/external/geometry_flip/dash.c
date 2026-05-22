#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>
#include <storage/storage.h>
#include <toolbox/stream/file_stream.h>
#include <string.h>
#include <m-array.h>

#define TAG "GeometryDash"

#define GD_APP_DATA_PATH EXT_PATH("apps_data/geometry_flip")

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define CUBE_SIZE 8
#define CUBE_X_POS 20
#define CUBE_JUMP_VEL -120.0f
#define CUBE_GRAVITY 250.0f
#define GROUND_Y (SCREEN_HEIGHT - 8)

#define SHIP_SIZE 8
#define SHIP_FLY_VEL -100.0f
#define SHIP_GRAVITY 150.0f
#define SHIP_MAX_FALLSPEED 200.0f

#define SCROLL_SPEED 60.0f
#define OBSTACLE_WIDTH CUBE_SIZE
#define OBSTACLE_HEIGHT CUBE_SIZE
#define OBSTACLE_GAP 20

#define COYOTE_TIME_MS 120

typedef enum {
    GameStateMenu,
    GameStatePlaying,
    GameStateGameOver,
    GameStateWin
} GameState;

typedef enum {
    PlayerModeCube,
    PlayerModeShip
} PlayerMode;

typedef enum {
    GravityDirectionDown,
    GravityDirectionUp
} GravityDirection;

typedef enum {
    ObstacleTypeNone = 0,
    ObstacleTypeBlock,
    ObstacleTypeSpikeUp,
    ObstacleTypeSpikeDown,
    ObstacleTypeSpikeLeft,
    ObstacleTypeSpikeRight,
    ObstacleTypePortalShip,
    ObstacleTypePortalCube,
    ObstacleTypeGravityUp,
    ObstacleTypeGravityDown,
    ObstacleTypeCount
} ObstacleType;

typedef struct {
    ObstacleType type;
    uint8_t grid_x;
    uint8_t grid_y;
    uint8_t length;
} Obstacle;

#define MAX_LEVEL_OBSTACLES 2000
typedef struct {
    Obstacle obstacles[MAX_LEVEL_OBSTACLES];
    int length;
    char name[32];
    int max_grid_x;
} LevelData;

ARRAY_DEF(LevelFileArray, char*, M_POD_OPLIST)

typedef struct {
    GameState state;
    PlayerMode player_mode;
    GravityDirection ship_gravity_dir;
    float cube_y;
    float cube_velocity_y;
    float scroll_offset;
    uint32_t last_tick;
    bool input_pressed;
    bool is_on_ground;
    uint32_t ground_lost_time;

    LevelData current_level;
    int selected_menu_item;
    LevelFileArray_t level_files;
} GeometryDashApp;

static void draw_cube(Canvas* canvas, float y) {
    canvas_draw_box(canvas, CUBE_X_POS, (int)y, CUBE_SIZE, CUBE_SIZE);
    canvas_draw_dot(canvas, CUBE_X_POS + 2, (int)y + 2);
    canvas_draw_dot(canvas, CUBE_X_POS + CUBE_SIZE - 3, (int)y + 2);
}

static void draw_ship(Canvas* canvas, float y, GravityDirection gravity_dir) {
    int ship_y = (int)y;
    int ship_bottom = ship_y + SHIP_SIZE;
    
    if (gravity_dir == GravityDirectionDown) {
        canvas_draw_line(canvas, CUBE_X_POS, ship_y + SHIP_SIZE/2, CUBE_X_POS + SHIP_SIZE, ship_y);
        canvas_draw_line(canvas, CUBE_X_POS, ship_y + SHIP_SIZE/2, CUBE_X_POS + SHIP_SIZE, ship_bottom);
        canvas_draw_line(canvas, CUBE_X_POS + SHIP_SIZE, ship_y, CUBE_X_POS + SHIP_SIZE, ship_bottom);
    } else {
        canvas_draw_line(canvas, CUBE_X_POS, ship_y + SHIP_SIZE/2, CUBE_X_POS + SHIP_SIZE, ship_y);
        canvas_draw_line(canvas, CUBE_X_POS, ship_y + SHIP_SIZE/2, CUBE_X_POS + SHIP_SIZE, ship_bottom);
        canvas_draw_line(canvas, CUBE_X_POS + SHIP_SIZE, ship_y, CUBE_X_POS + SHIP_SIZE, ship_bottom);
    }
}

static void draw_player(GeometryDashApp* app, Canvas* canvas) {
    if (app->player_mode == PlayerModeCube) {
        draw_cube(canvas, app->cube_y);
    } else if (app->player_mode == PlayerModeShip) {
        draw_ship(canvas, app->cube_y, app->ship_gravity_dir);
    }
}

static void draw_vertical_special_object(Canvas* canvas, int screen_x, int base_grid_y, ObstacleType type) {
    for (int i = 0; i < 3; i++) {
        int y = base_grid_y + i;
        int screen_y = GROUND_Y - (y + 1) * OBSTACLE_HEIGHT;
        
        canvas_draw_frame(canvas, screen_x, screen_y, OBSTACLE_WIDTH, OBSTACLE_HEIGHT);
        
        if (i == 1) {
            switch (type) {
                case ObstacleTypePortalShip:
                    canvas_draw_line(canvas, screen_x, screen_y, screen_x + OBSTACLE_WIDTH, screen_y + OBSTACLE_HEIGHT);
                    canvas_draw_line(canvas, screen_x + OBSTACLE_WIDTH, screen_y, screen_x, screen_y + OBSTACLE_HEIGHT);
                    break;
                case ObstacleTypePortalCube:
                    canvas_draw_disc(canvas, screen_x + OBSTACLE_WIDTH/2, screen_y + OBSTACLE_HEIGHT/2, 2);
                    break;
                case ObstacleTypeGravityUp:
                    canvas_draw_line(canvas, screen_x + OBSTACLE_WIDTH/2, screen_y + 2, screen_x + OBSTACLE_WIDTH/2, screen_y + OBSTACLE_HEIGHT - 2);
                    canvas_draw_line(canvas, screen_x + OBSTACLE_WIDTH/2, screen_y + 2, screen_x + OBSTACLE_WIDTH/2 - 2, screen_y + 4);
                    canvas_draw_line(canvas, screen_x + OBSTACLE_WIDTH/2, screen_y + 2, screen_x + OBSTACLE_WIDTH/2 + 2, screen_y + 4);
                    break;
                case ObstacleTypeGravityDown:
                    canvas_draw_line(canvas, screen_x + OBSTACLE_WIDTH/2, screen_y + 2, screen_x + OBSTACLE_WIDTH/2, screen_y + OBSTACLE_HEIGHT - 2);
                    canvas_draw_line(canvas, screen_x + OBSTACLE_WIDTH/2, screen_y + OBSTACLE_HEIGHT - 2, screen_x + OBSTACLE_WIDTH/2 - 2, screen_y + OBSTACLE_HEIGHT - 4);
                    canvas_draw_line(canvas, screen_x + OBSTACLE_WIDTH/2, screen_y + OBSTACLE_HEIGHT - 2, screen_x + OBSTACLE_WIDTH/2 + 2, screen_y + OBSTACLE_HEIGHT - 4);
                    break;
                default:
                    break;
            }
        }
    }
}

static void draw_filled_spike(Canvas* canvas, int screen_x, int screen_y, ObstacleType type) {
    switch (type) {
        case ObstacleTypeSpikeUp: {
            for (int y = screen_y; y < screen_y + OBSTACLE_HEIGHT; y++) {
                int height = y - screen_y;
                int width = (OBSTACLE_WIDTH * height) / OBSTACLE_HEIGHT;
                int left_x = screen_x + (OBSTACLE_WIDTH - width) / 2;
                canvas_draw_line(canvas, left_x, y, left_x + width, y);
            }
            break;
        }
        case ObstacleTypeSpikeDown: {
            screen_y -= OBSTACLE_HEIGHT;
            
            for (int y = screen_y; y < screen_y + OBSTACLE_HEIGHT; y++) {
                int height = (screen_y + OBSTACLE_HEIGHT) - y;
                int width = (OBSTACLE_WIDTH * height) / OBSTACLE_HEIGHT;
                int left_x = screen_x + (OBSTACLE_WIDTH - width) / 2;
                canvas_draw_line(canvas, left_x, y, left_x + width, y);
            }
            break;
        }
        case ObstacleTypeSpikeLeft: {
            for (int x = screen_x; x < screen_x + OBSTACLE_WIDTH; x++) {
                int width = x - screen_x;
                int height = (OBSTACLE_HEIGHT * width) / OBSTACLE_WIDTH;
                int top_y = screen_y + (OBSTACLE_HEIGHT - height) / 2;
                canvas_draw_line(canvas, x, top_y, x, top_y + height);
            }
            break;
        }
        case ObstacleTypeSpikeRight: {
            for (int x = screen_x; x < screen_x + OBSTACLE_WIDTH; x++) {
                int width = (screen_x + OBSTACLE_WIDTH) - x;
                int height = (OBSTACLE_HEIGHT * width) / OBSTACLE_WIDTH;
                int top_y = screen_y + (OBSTACLE_HEIGHT - height) / 2;
                canvas_draw_line(canvas, x, top_y, x, top_y + height);
            }
            break;
        }
        default:
            break;
    }
}

static void draw_obstacle(Canvas* canvas, int screen_x, int grid_y, ObstacleType type) {
    if (type == ObstacleTypeNone) return;

    bool is_special_object = (type == ObstacleTypePortalShip || type == ObstacleTypePortalCube ||
                              type == ObstacleTypeGravityUp || type == ObstacleTypeGravityDown);

    if (is_special_object) {
        draw_vertical_special_object(canvas, screen_x, grid_y, type);
        return;
    }

    int screen_y = 0;
    
    if (type == ObstacleTypeSpikeUp) {
        screen_y = GROUND_Y - (grid_y + 1) * OBSTACLE_HEIGHT;
    } else if (type == ObstacleTypeSpikeDown) {
        screen_y = GROUND_Y - (grid_y + 1) * OBSTACLE_HEIGHT + OBSTACLE_HEIGHT;
    }

    switch (type) {
        case ObstacleTypeSpikeUp:
        case ObstacleTypeSpikeDown:
        case ObstacleTypeSpikeLeft:
        case ObstacleTypeSpikeRight:
            draw_filled_spike(canvas, screen_x, screen_y, type);
            break;
        default:
            break;
    }
}

static void draw_ground(Canvas* canvas) {
    canvas_draw_line(canvas, 0, GROUND_Y, SCREEN_WIDTH, GROUND_Y);
}

static void draw_ui(Canvas* canvas, GeometryDashApp* app) {
    char buffer[64];
    int progress = (int)((app->scroll_offset / (app->current_level.max_grid_x * OBSTACLE_GAP)) * 100);
    if (progress > 100) progress = 100;
    snprintf(buffer, sizeof(buffer), "Progress: %d%%", progress);
    canvas_draw_str(canvas, 2, 10, buffer);

    if(app->state == GameStatePlaying || app->state == GameStateGameOver || app->state == GameStateWin) {
        snprintf(buffer, sizeof(buffer), "Level: %s", app->current_level.name);
        canvas_draw_str(canvas, 2, 20, buffer);
    }
}

static void render_callback(Canvas* canvas, void* ctx) {
    GeometryDashApp* app = (GeometryDashApp*)ctx;
    furi_assert(app);

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    if (app->state == GameStateMenu) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 5, AlignCenter, AlignTop, "Geometry Dash");
        canvas_set_font(canvas, FontSecondary);

        size_t file_count = LevelFileArray_size(app->level_files);
        if (file_count == 0) {
             canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignTop, "No levels found!");
             canvas_draw_str_aligned(canvas, 64, 45, AlignCenter, AlignTop, "Put .gflvl files in");
             canvas_draw_str_aligned(canvas, 64, 55, AlignCenter, AlignTop, GD_APP_DATA_PATH);
             return;
        }

        int start_index = MAX(0, app->selected_menu_item - 2);
        int end_index = MIN((int)file_count, start_index + 5);

        for (int i = start_index; i < end_index; i++) {
            int y_pos = 20 + (i - start_index) * 12;
            if (i == app->selected_menu_item) {
                canvas_draw_str(canvas, 2, y_pos, ">");
            }
            char** filename_ptr = LevelFileArray_get(app->level_files, i);
            if(filename_ptr) {
                char* filename = *filename_ptr;
                char display_name[32];
                strncpy(display_name, filename, sizeof(display_name) - 1);
                display_name[sizeof(display_name) - 1] = '\0';
                char* dot = strrchr(display_name, '.');
                if(dot) *dot = '\0';

                canvas_draw_str(canvas, 15, y_pos, display_name);
            }
        }
        return;
    }

    if (app->state == GameStateGameOver) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignTop, "Game Over!");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignTop, "Press OK to Restart");
        canvas_draw_str_aligned(canvas, 64, 55, AlignCenter, AlignTop, "Press BACK to Menu");
        return;
    }

    if (app->state == GameStateWin) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignTop, "Level Complete!");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignTop, "Press OK to Restart");
        canvas_draw_str_aligned(canvas, 64, 55, AlignCenter, AlignTop, "Press BACK to Menu");
        return;
    }

    draw_ground(canvas);
    draw_player(app, canvas);
    draw_ui(canvas, app);

    int start_grid_x = (int)(app->scroll_offset / OBSTACLE_GAP) - 2;
    int end_grid_x = start_grid_x + (SCREEN_WIDTH / OBSTACLE_GAP) + 4;

    for (int grid_x = start_grid_x; grid_x <= end_grid_x; grid_x++) {
        int screen_x = (grid_x * OBSTACLE_GAP) - (int)app->scroll_offset;

        for (int j = 0; j < app->current_level.length; j++) {
            Obstacle* obs = &app->current_level.obstacles[j];
            if (obs->grid_x == grid_x) {
                if (obs->type == ObstacleTypeBlock) {
                    int platform_width = obs->length * OBSTACLE_WIDTH;
                    int screen_y = GROUND_Y - (obs->grid_y + 1) * OBSTACLE_HEIGHT;
                    canvas_draw_box(canvas, screen_x, screen_y, platform_width, OBSTACLE_HEIGHT);
                } 
                else {
                    draw_obstacle(canvas, screen_x, obs->grid_y, obs->type);
                }
            }
        }
    }
}

static void input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    FuriMessageQueue* event_queue = ctx;
    furi_message_queue_put(event_queue, input_event, FuriWaitForever);
}

static void process_game_logic(GeometryDashApp* app, uint32_t dt_ms) {
    if (app->state != GameStatePlaying) return;

    float dt = dt_ms / 1000.0f;

    app->scroll_offset += SCROLL_SPEED * dt;
    if (app->scroll_offset >= app->current_level.max_grid_x * OBSTACLE_GAP) {
        app->state = GameStateWin;
        return;
    }

    if (app->player_mode == PlayerModeCube) {
        app->cube_velocity_y += CUBE_GRAVITY * dt;
        app->cube_y += app->cube_velocity_y * dt;

        bool was_on_ground = app->is_on_ground;
        app->is_on_ground = false;

        if (app->cube_y > GROUND_Y - CUBE_SIZE) {
            app->cube_y = GROUND_Y - CUBE_SIZE;
            app->cube_velocity_y = 0.0f;
            app->is_on_ground = true;
            if (!was_on_ground) {
                app->ground_lost_time = furi_get_tick();
            }
        }

        if (app->cube_y < 0) {
            app->cube_y = 0;
            app->cube_velocity_y = 0.0f;
        }

    } else if (app->player_mode == PlayerModeShip) {
        float current_gravity = (app->ship_gravity_dir == GravityDirectionDown) ? SHIP_GRAVITY : -SHIP_GRAVITY;
        app->cube_velocity_y += current_gravity * dt;
        
        if (app->ship_gravity_dir == GravityDirectionDown && app->cube_velocity_y > SHIP_MAX_FALLSPEED) {
            app->cube_velocity_y = SHIP_MAX_FALLSPEED;
        } else if (app->ship_gravity_dir == GravityDirectionUp && app->cube_velocity_y < -SHIP_MAX_FALLSPEED) {
            app->cube_velocity_y = -SHIP_MAX_FALLSPEED;
        }
        
        app->cube_y += app->cube_velocity_y * dt;

        if (app->ship_gravity_dir == GravityDirectionDown) {
            if (app->cube_y > GROUND_Y - SHIP_SIZE) {
                app->cube_y = GROUND_Y - SHIP_SIZE;
                app->cube_velocity_y = 0.0f;
            }
            if (app->cube_y < 0) {
                app->cube_y = 0;
                app->cube_velocity_y = 0.0f;
            }
        } else {
            if (app->cube_y < 0) {
                app->cube_y = 0;
                app->cube_velocity_y = 0.0f;
            }
            if (app->cube_y > GROUND_Y - SHIP_SIZE) {
                app->cube_y = GROUND_Y - SHIP_SIZE;
                app->cube_velocity_y = 0.0f;
            }
        }
    }

    int player_screen_x = CUBE_X_POS;
    int player_screen_y = (int)app->cube_y;
    int player_right = player_screen_x + ((app->player_mode == PlayerModeCube) ? CUBE_SIZE : SHIP_SIZE);
    int player_bottom = player_screen_y + ((app->player_mode == PlayerModeCube) ? CUBE_SIZE : SHIP_SIZE);

    int start_grid_x = (int)(app->scroll_offset / OBSTACLE_GAP) - 1;
    int end_grid_x = start_grid_x + (SCREEN_WIDTH / OBSTACLE_GAP) + 2;

    for (int grid_x = start_grid_x; grid_x <= end_grid_x; grid_x++) {
        int obstacle_screen_x = (grid_x * OBSTACLE_GAP) - (int)app->scroll_offset;

        for (int j = 0; j < app->current_level.length; j++) {
            Obstacle* obstacle = &app->current_level.obstacles[j];
            if (obstacle->grid_x == grid_x && obstacle->type != ObstacleTypeNone) {

                bool is_special_object = (obstacle->type == ObstacleTypePortalShip || obstacle->type == ObstacleTypePortalCube ||
                                          obstacle->type == ObstacleTypeGravityUp || obstacle->type == ObstacleTypeGravityDown);

                bool collision = false;

                if (is_special_object) {
                    for (int k = 0; k < 3; k++) {
                        int block_y = obstacle->grid_y + k;
                        int block_screen_y = GROUND_Y - (block_y + 1) * OBSTACLE_HEIGHT;
                        int block_bottom = block_screen_y + OBSTACLE_HEIGHT;

                        if (player_screen_x < obstacle_screen_x + OBSTACLE_WIDTH && player_right > obstacle_screen_x &&
                            player_screen_y < block_bottom && player_bottom > block_screen_y) {
                            collision = true;
                            break;
                        }
                    }
                    
                    if (collision) {
                        switch (obstacle->type) {
                            case ObstacleTypePortalShip:
                                app->player_mode = PlayerModeShip;
                                app->cube_velocity_y = 0.0f;
                                app->is_on_ground = false;
                                return;
                            case ObstacleTypePortalCube:
                                app->player_mode = PlayerModeCube;
                                app->cube_velocity_y = 0.0f;
                                app->is_on_ground = false;
                                return;
                            case ObstacleTypeGravityUp:
                                if (app->player_mode == PlayerModeShip) {
                                    app->ship_gravity_dir = GravityDirectionUp;
                                }
                                return;
                            case ObstacleTypeGravityDown:
                                if (app->player_mode == PlayerModeShip) {
                                    app->ship_gravity_dir = GravityDirectionDown;
                                }
                                return;
                            default:
                                app->state = GameStateGameOver;
                                return;
                        }
                    }
                } else {
                    int obstacle_screen_y = 0;
                    int obstacle_bottom = 0;
                    int obstacle_right = 0;

                    bool is_on_ground = (obstacle->type == ObstacleTypeBlock || obstacle->type == ObstacleTypeSpikeUp);
                    
                    if (is_on_ground) {
                        obstacle_screen_y = GROUND_Y - (obstacle->grid_y + 1) * OBSTACLE_HEIGHT;
                    } else if (obstacle->type == ObstacleTypeSpikeDown) {
                        obstacle_screen_y = GROUND_Y - (obstacle->grid_y + 1) * OBSTACLE_HEIGHT + OBSTACLE_HEIGHT;
                    } else if (obstacle->type == ObstacleTypeSpikeLeft || obstacle->type == ObstacleTypeSpikeRight) {
                        obstacle_screen_y = GROUND_Y - (obstacle->grid_y + 1) * OBSTACLE_HEIGHT;
                    }
                    
                    if (obstacle->type == ObstacleTypeBlock) {
                        obstacle_right = obstacle_screen_x + (obstacle->length * OBSTACLE_WIDTH);
                        obstacle_bottom = obstacle_screen_y + OBSTACLE_HEIGHT;
                    } else {
                        obstacle_right = obstacle_screen_x + OBSTACLE_WIDTH;
                        obstacle_bottom = obstacle_screen_y + ((obstacle->type == ObstacleTypeSpikeDown || 
                                                               obstacle->type == ObstacleTypeSpikeLeft || 
                                                               obstacle->type == ObstacleTypeSpikeRight) ? 0 : OBSTACLE_HEIGHT);
                    }

                    if (player_screen_x < obstacle_right && player_right > obstacle_screen_x &&
                        player_screen_y < obstacle_bottom && player_bottom > obstacle_screen_y) {

                        if (obstacle->type == ObstacleTypeBlock) {
                            if (app->player_mode == PlayerModeCube) {
                                int overlap_left = player_right - obstacle_screen_x;
                                int overlap_right = obstacle_right - player_screen_x;
                                int overlap_top = player_bottom - obstacle_screen_y;
                                int overlap_bottom = obstacle_bottom - player_screen_y;

                                int min_overlap = MIN(MIN(overlap_left, overlap_right), MIN(overlap_top, overlap_bottom));

                                if (min_overlap == overlap_top && app->cube_velocity_y >= 0) {
                                    app->cube_y = obstacle_screen_y - CUBE_SIZE;
                                    app->cube_velocity_y = 0.0f;
                                    bool was_on_ground = app->is_on_ground;
                                    app->is_on_ground = true;
                                    if (!was_on_ground) {
                                        app->ground_lost_time = furi_get_tick();
                                    }
                                    collision = false;
                                } else if (min_overlap == overlap_bottom && app->cube_velocity_y <= 0) {
                                    app->cube_y = obstacle_bottom;
                                    app->cube_velocity_y = 0.0f;
                                    collision = false;
                                } else {
                                    collision = true;
                                }
                            } else if (app->player_mode == PlayerModeShip) {
                                collision = true;
                            }
                        } else if (obstacle->type == ObstacleTypeSpikeUp || 
                                   obstacle->type == ObstacleTypeSpikeDown ||
                                   obstacle->type == ObstacleTypeSpikeLeft ||
                                   obstacle->type == ObstacleTypeSpikeRight) {
                            collision = true;
                        }

                        if (collision) {
                            app->state = GameStateGameOver;
                            return;
                        }
                    }
                }
            }
        }
    }
}

static ObstacleType char_to_obstacle_type(char c) {
    switch(c) {
        case '0': return ObstacleTypeNone;
        case '1': return ObstacleTypeBlock;
        case '2': return ObstacleTypeSpikeUp;
        case '3': return ObstacleTypeSpikeDown;
        case '4': return ObstacleTypeSpikeLeft;
        case '5': return ObstacleTypeSpikeRight;
        case '6': return ObstacleTypePortalShip;
        case '7': return ObstacleTypePortalCube;
        case '8': return ObstacleTypeGravityUp;
        case '9': return ObstacleTypeGravityDown;
        default: return ObstacleTypeNone;
    }
}

static int safe_atoi(const char* str) {
    if (!str) return 0;
    while (*str == ' ') str++;
    if (*str == '\0' || (!isdigit((unsigned char)*str) && *str != '-' && *str != '+')) {
        return 0;
    }
    return atoi(str);
}

static bool load_level_from_file(GeometryDashApp* app, const char* filename) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    Stream* stream = file_stream_alloc(storage);

    FURI_LOG_I(TAG, "Loading level: %s", filename);
    char full_path[256];
    snprintf(full_path, sizeof(full_path), "%s/%s", GD_APP_DATA_PATH, filename);

    bool success = false;
    if(file_stream_open(stream, full_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FuriString* line;
        line = furi_string_alloc();

        int index = 0;
        int max_x = 0;
        while(stream_read_line(stream, line) && index < MAX_LEVEL_OBSTACLES) {
            const char* line_str = furi_string_get_cstr(line);
            size_t line_len = strlen(line_str);

            if(line_len > 0 && line_str[0] != '#' && line_str[0] != '\n' && line_str[0] != '\r') {
                
                char command_char = line_str[0];
                
                char* commas[4] = {0};
                int comma_count = 0;
                char* temp = strchr(line_str, ',');
                while(temp && comma_count < 3) {
                    commas[comma_count++] = temp;
                    temp = strchr(temp + 1, ',');
                }
                
                if (comma_count >= 2) {
                    *commas[0] = '\0';
                    *commas[1] = '\0';
                    
                    int x = safe_atoi(commas[0] + 1);
                    int y = safe_atoi(commas[1] + 1);
                    int length_or_height = 1;
                    
                    if (comma_count >= 3) {
                        *commas[2] = '\0';
                        length_or_height = safe_atoi(commas[2] + 1);
                    }
                    
                    if (length_or_height < 1) length_or_height = 1;
                    if (length_or_height > 100) length_or_height = 100;

                    if (command_char == 'b') {
                        if (index < MAX_LEVEL_OBSTACLES) {
                            Obstacle* obs = &app->current_level.obstacles[index];
                            obs->type = ObstacleTypeBlock;
                            obs->grid_x = x;
                            obs->grid_y = y;
                            obs->length = length_or_height;
                            index++;
                            int last_x = x + length_or_height - 1;
                            if (last_x > max_x) max_x = last_x;
                        }
                    } else if (command_char == 'w') {
                        if (index < MAX_LEVEL_OBSTACLES) {
                            Obstacle* obs = &app->current_level.obstacles[index];
                            obs->type = ObstacleTypeBlock;
                            obs->grid_x = x;
                            obs->grid_y = y;
                            obs->length = length_or_height;
                            index++;
                            if (x > max_x) max_x = x;
                        }
                    } else {
                        ObstacleType type = char_to_obstacle_type(command_char);
                        if (type != ObstacleTypeNone) {
                            if (index < MAX_LEVEL_OBSTACLES) {
                                Obstacle* obs = &app->current_level.obstacles[index];
                                obs->type = type;
                                obs->grid_x = x;
                                obs->grid_y = y;
                                obs->length = 1;
                                index++;
                                if (x > max_x) max_x = x;
                            }
                        }
                    }
                }
            }
        }
        
        app->current_level.length = index;
        app->current_level.max_grid_x = max_x + 10; 
        strncpy(app->current_level.name, filename, sizeof(app->current_level.name) - 1);
        app->current_level.name[sizeof(app->current_level.name) - 1] = '\0';
        char* dot = strrchr(app->current_level.name, '.');
        if(dot) *dot = '\0';

        furi_string_free(line);
        success = true;
        FURI_LOG_I(TAG, "Loaded level: %s (%d obstacles, max_x=%d)", filename, index, max_x);
    } else {
        FURI_LOG_E(TAG, "Failed to open level file: %s", full_path);
    }

    file_stream_close(stream);
    stream_free(stream);
    furi_record_close(RECORD_STORAGE);

    return success;
}


static void free_level_files(LevelFileArray_t* level_files) {
    for(size_t i = 0; i < LevelFileArray_size(*level_files); i++) {
        char** filename_ptr = LevelFileArray_get(*level_files, i);
        if(filename_ptr) {
            free(*filename_ptr);
        }
    }
    LevelFileArray_clear(*level_files);
}

static void scan_level_files(GeometryDashApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);

    FS_Error mkdir_result = storage_common_mkdir(storage, GD_APP_DATA_PATH);
    if(mkdir_result != FSE_OK && mkdir_result != FSE_EXIST) {
        FURI_LOG_E(TAG, "Failed to create directory %s: %d", GD_APP_DATA_PATH, mkdir_result);
    }

    File* dir = storage_file_alloc(storage);
    if(storage_dir_open(dir, GD_APP_DATA_PATH)) {
        FileInfo fileinfo;
        char name[256];

        free_level_files(&app->level_files);
        LevelFileArray_init(app->level_files);

        while(storage_dir_read(dir, &fileinfo, name, sizeof(name))) {
            if(!(fileinfo.flags & FSF_DIRECTORY)) {
                const char *dot = strrchr(name, '.');
                if(dot && strcmp(dot, ".gflvl") == 0) {
                    char* name_copy = strdup(name);
                    LevelFileArray_push_back(app->level_files, name_copy);
                }
            }
        }
        FURI_LOG_I(TAG, "Found %zu level files", LevelFileArray_size(app->level_files));
    } else {
        FURI_LOG_E(TAG, "Failed to open directory %s", GD_APP_DATA_PATH);
    }

    storage_dir_close(dir);
    storage_file_free(dir);
    furi_record_close(RECORD_STORAGE);
}

static void reset_game(GeometryDashApp* app) {
    app->player_mode = PlayerModeCube;
    app->ship_gravity_dir = GravityDirectionDown;
    app->cube_y = GROUND_Y - CUBE_SIZE;
    app->cube_velocity_y = 0.0f;
    app->scroll_offset = 0.0f;
    app->is_on_ground = true;
    app->input_pressed = false;
    app->ground_lost_time = furi_get_tick();
    app->last_tick = furi_get_tick();
    app->state = GameStatePlaying;
    FURI_LOG_I(TAG, "Game reset complete");
}

int32_t geometry_dash_app(void* p) {
    UNUSED(p);
    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    GeometryDashApp* app = malloc(sizeof(GeometryDashApp));
    memset(app, 0, sizeof(GeometryDashApp));
    app->state = GameStateMenu;
    app->player_mode = PlayerModeCube;
    app->ship_gravity_dir = GravityDirectionDown;
    app->cube_y = GROUND_Y - CUBE_SIZE;
    app->cube_velocity_y = 0.0f;
    app->scroll_offset = 0.0f;
    app->last_tick = furi_get_tick();
    app->input_pressed = false;
    app->selected_menu_item = 0;
    app->is_on_ground = true;
    app->ground_lost_time = furi_get_tick();
    LevelFileArray_init(app->level_files);

    scan_level_files(app);

    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, render_callback, app);
    view_port_input_callback_set(view_port, input_callback, event_queue);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    InputEvent event;
    bool running = true;

    while(running) {
        FuriStatus event_status = furi_message_queue_get(event_queue, &event, 50);

        if(event_status == FuriStatusOk) {
            if(event.type == InputTypePress || event.type == InputTypeRepeat) {
                if (app->state == GameStateMenu) {
                    size_t file_count = LevelFileArray_size(app->level_files);
                    if(file_count > 0) {
                        if(event.key == InputKeyUp) {
                            app->selected_menu_item = (app->selected_menu_item - 1 + file_count) % file_count;
                        } else if(event.key == InputKeyDown) {
                            app->selected_menu_item = (app->selected_menu_item + 1) % file_count;
                        } else if(event.key == InputKeyOk) {
                            char** selected_filename_ptr = LevelFileArray_get(app->level_files, app->selected_menu_item);
                            if(selected_filename_ptr) {
                                char* selected_filename = *selected_filename_ptr;
                                if(load_level_from_file(app, selected_filename)) {
                                    reset_game(app);
                                } else {
                                    FURI_LOG_E(TAG, "Failed to load level: %s", selected_filename);
                                }
                            }
                        }
                    }
                    if(event.key == InputKeyBack) {
                        running = false;
                    }
                } else if (app->state == GameStatePlaying) {
                    if(event.key == InputKeyOk) {
                        if (app->player_mode == PlayerModeCube) {
                            uint32_t current_time = furi_get_tick();
                            uint32_t time_since_ground_lost = current_time - app->ground_lost_time;

                            bool can_jump = app->is_on_ground ||
                                           (!app->is_on_ground && time_since_ground_lost < COYOTE_TIME_MS);

                            if (can_jump && !app->input_pressed) {
                                app->cube_velocity_y = CUBE_JUMP_VEL;
                                app->input_pressed = true;
                            }
                        } else if (app->player_mode == PlayerModeShip) {
                            if (!app->input_pressed) {
                                float fly_vel = (app->ship_gravity_dir == GravityDirectionDown) ? SHIP_FLY_VEL : -SHIP_FLY_VEL;
                                app->cube_velocity_y = fly_vel;
                                app->input_pressed = true;
                            }
                        }
                    } else if(event.key == InputKeyBack) {
                         app->state = GameStateMenu;
                         scan_level_files(app);
                    }
                } else if (app->state == GameStateGameOver || app->state == GameStateWin) {
                     if(event.key == InputKeyOk) {
                         char** selected_filename_ptr = LevelFileArray_get(app->level_files, app->selected_menu_item);
                         if(selected_filename_ptr) {
                             char* selected_filename = *selected_filename_ptr;
                             if(load_level_from_file(app, selected_filename)) {
                                reset_game(app);
                             } else {
                                FURI_LOG_E(TAG, "Failed to reload level: %s", selected_filename);
                             }
                         }
                     } else if(event.key == InputKeyBack) {
                         app->state = GameStateMenu;
                         scan_level_files(app);
                     }
                }
            } else if (event.type == InputTypeRelease && event.key == InputKeyOk) {
                 app->input_pressed = false;
            }
        }

        uint32_t current_tick = furi_get_tick();
        uint32_t dt_ms = current_tick - app->last_tick;
        if (dt_ms > 0 && dt_ms < 100) {
            process_game_logic(app, dt_ms);
            app->last_tick = current_tick;
        }

        view_port_update(view_port);
    }

    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_message_queue_free(event_queue);

    free_level_files(&app->level_files);
    LevelFileArray_clear(app->level_files);

    furi_record_close(RECORD_GUI);
    free(app);

    return 0;
}
