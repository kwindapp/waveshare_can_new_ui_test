/* RWB JANINE - exact artwork as RGB565 LVGL background, LVGL 8.4, 800x480.
   Retains EEZ objects and the MODE 2 navigation label expected by main.cpp. */
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include "screens.h"
#include "images.h"
#include "fonts.h"
extern const lv_font_t ui_font_km_136;
#include "actions.h"
#include "vars.h"
#include "styles.h"
#include "ui.h"

LV_IMG_DECLARE(rwb_janine_bg);
LV_FONT_DECLARE(ui_font_Hollow85);
LV_FONT_DECLARE(ui_font_Hollow38);
extern bool TEST_MODE;
extern void open_trijekt_mode2(void);
extern void open_trijekt_mode3(void);
objects_t objects;
lv_obj_t *tick_value_change_obj;
uint32_t active_theme_index = 0;
uint32_t theme_colors[1][2] = {{0xffff334b, 0xff030408}};

#define TACH_SEGMENTS 80
static lv_obj_t *tach_segments[TACH_SEGMENTS];
static lv_point_t tach_points[TACH_SEGMENTS][2];
static lv_obj_t *battery_value = NULL;
static lv_obj_t *temp_value = NULL;
static bool entrance_done = false;
static int last_tach_active = -1;
static lv_obj_t *rpm_pointer = NULL;
static lv_point_t rpm_pointer_points[2];

/* Chrome RPM entrance: slide upward whenever the dashboard opens.
   Subsequent CAN RPM updates change the text without restarting movement. */
#define RPM_LABEL_Y 135
#define RPM_RISE_PIXELS 65
static void rpm_anim_y(void *obj, int32_t y) {
    lv_obj_set_y((lv_obj_t *)obj, (lv_coord_t)y);
}
static void rpm_start_entrance(void) {
    if (!objects.label_rpm) return;
    lv_anim_del(objects.label_rpm, rpm_anim_y);
    lv_obj_set_y(objects.label_rpm, RPM_LABEL_Y + RPM_RISE_PIXELS);
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, objects.label_rpm);
    lv_anim_set_exec_cb(&anim, rpm_anim_y);
    lv_anim_set_values(&anim, RPM_LABEL_Y + RPM_RISE_PIXELS, RPM_LABEL_Y);
    lv_anim_set_time(&anim, 1650);
    lv_anim_set_delay(&anim, 580);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
    lv_anim_start(&anim);
}

static lv_obj_t *make_label(lv_obj_t *parent, const char *s, int x, int y,
                            int w, const lv_font_t *font, uint32_t color) {
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, s);
    lv_obj_set_pos(l, x, y);
    lv_obj_set_width(l, w);
    lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_font(l, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(l, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(l, LV_OPA_TRANSP, LV_PART_MAIN);
    return l;
}

static void anim_x(void *o, int32_t v) { lv_obj_set_x((lv_obj_t *)o, (lv_coord_t)v); }
static void animate_from_center(lv_obj_t *o, int end_x, uint32_t delay) {
    if (!o) return;
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, o);
    lv_anim_set_exec_cb(&a, anim_x);
    lv_anim_set_values(&a, 400 - lv_obj_get_width(o) / 2, end_x);
    lv_anim_set_time(&a, 900);
    lv_anim_set_delay(&a, delay);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

static void screen_event(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_SCREEN_LOADED) {
        rpm_start_entrance();
        if (!entrance_done) {
            entrance_done = true;
            animate_from_center(battery_value, 164, 200);
            animate_from_center(temp_value, 551, 200);
        }
    }
}

static void nav_event(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) open_trijekt_mode2();
}

/* A lightweight segmented highlight follows the curved printed tachometer.
   Background artwork remains unchanged; only the illuminated trace moves. */
/* Tachometer follows the artwork's curved 0-8000 scale.
   Update only segments that actually changed, avoiding 80 style writes each frame. */
void rwb_set_tach_rpm(uint16_t rpm) {
    if (rpm > 8000) rpm = 8000;
    int active = ((uint32_t)rpm * TACH_SEGMENTS + 7999) / 8000;
    if (active == last_tach_active) return;
    int old = last_tach_active < 0 ? 0 : last_tach_active;
    int lo = old < active ? old : active;
    int hi = old > active ? old : active;
    for (int i = lo; i < hi; ++i) {
        if (!tach_segments[i]) continue;
        lv_obj_set_style_line_opa(tach_segments[i], i < active ? LV_OPA_COVER : LV_OPA_TRANSP, LV_PART_MAIN);
    }
    last_tach_active = active;
    if (rpm_pointer) {
        /* Moving bright pointer at the leading edge of the illuminated scale. */
        float u = (float)rpm / 8000.0f;
        float x = 81.0f + 637.0f * u;
        float dx = x - 400.0f;
        float y = 132.0f + 0.00042f * dx * dx;
        rpm_pointer_points[0] = (lv_point_t){(lv_coord_t)x, (lv_coord_t)(y - 9)};
        rpm_pointer_points[1] = (lv_point_t){(lv_coord_t)x, (lv_coord_t)(y + 7)};
        lv_line_set_points(rpm_pointer, rpm_pointer_points, 2);
        lv_obj_set_style_line_color(rpm_pointer, lv_color_hex(rpm >= 6500 ? 0xFF324D : 0xF8FDFF), LV_PART_MAIN);
        if (rpm == 0) lv_obj_add_flag(rpm_pointer, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_clear_flag(rpm_pointer, LV_OBJ_FLAG_HIDDEN);
    }
}

static void touch_nav_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    int target = (int)(intptr_t)lv_event_get_user_data(e);
    if (target == 1) {
        /* Dashboard is MODE 1; already active when this button is visible. */
        if (objects.main && lv_scr_act() != objects.main) lv_scr_load(objects.main);
    }
    else if (target == 2 || target == 4) open_trijekt_mode2();
    else if (target == 3) open_trijekt_mode3();
}

static void make_touch_button(lv_obj_t *root, int x, int target, const char *title,
                              const char *sub, uint32_t accent) {
    lv_obj_t *b = lv_btn_create(root);
    lv_obj_set_pos(b, x, 439);
    lv_obj_set_size(b, 146, 35);
    lv_obj_set_style_radius(b, 9, LV_PART_MAIN);
    lv_obj_set_style_bg_color(b, lv_color_hex(0x080D16), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(b, LV_OPA_90, LV_PART_MAIN);
    lv_obj_set_style_border_width(b, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(b, lv_color_hex(accent), LV_PART_MAIN);
    lv_obj_set_style_shadow_width(b, 7, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(b, lv_color_hex(accent), LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(b, LV_OPA_30, LV_PART_MAIN);
    lv_obj_set_style_bg_color(b, lv_color_hex(accent), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(b, LV_OPA_50, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_pad_all(b, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(b, touch_nav_event, LV_EVENT_CLICKED, (void *)(intptr_t)target);
    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, title);
    lv_obj_set_style_text_font(l, &ui_font_t20, LV_PART_MAIN);
    lv_obj_set_style_text_color(l, lv_color_white(), LV_PART_MAIN);
    lv_obj_center(l);
    /* Keep a searchable MODE 2 label for the existing main.cpp hook.
       It is hidden, but the parent button is already wired directly. */
    if (target == 2) {
        lv_obj_t *compat = lv_label_create(b);
        lv_label_set_text(compat, "MODE 2");
        lv_obj_add_flag(compat, LV_OBJ_FLAG_HIDDEN);
    }
    (void)sub;
}

void create_screen_main(void) {
    lv_obj_t *root = lv_obj_create(NULL);
    objects.main = root;
    lv_obj_set_size(root, 800, 480);
    lv_obj_set_style_bg_color(root, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(root, 0, LV_PART_MAIN);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(root, screen_event, LV_EVENT_SCREEN_LOADED, NULL);

    lv_obj_t *background = lv_img_create(root);
    lv_img_set_src(background, &rwb_janine_bg);
    lv_obj_set_pos(background, 0, 0);
    lv_obj_clear_flag(background, LV_OBJ_FLAG_CLICKABLE);

    /* Live tachometer overlay approximates the upper curved scale in the artwork. */
    for (int i = 0; i < TACH_SEGMENTS; ++i) {
        float u0 = (float)i / TACH_SEGMENTS;
        float u1 = (float)(i + 1) / TACH_SEGMENTS;
        int x0 = 81 + (int)(637.0f * u0);
        int x1 = 81 + (int)(637.0f * u1);
        float dx0 = (float)(x0 - 400), dx1 = (float)(x1 - 400);
        int y0 = 132 + (int)(0.00042f * dx0 * dx0);
        int y1 = 132 + (int)(0.00042f * dx1 * dx1);
        tach_points[i][0] = (lv_point_t){(lv_coord_t)x0, (lv_coord_t)y0};
        tach_points[i][1] = (lv_point_t){(lv_coord_t)x1, (lv_coord_t)y1};
        lv_obj_t *seg = lv_line_create(root);
        tach_segments[i] = seg;
        lv_line_set_points(seg, tach_points[i], 2);
        lv_obj_set_style_line_color(seg, lv_color_hex(i >= 65 ? 0xFF304A : 0x32E6FF), LV_PART_MAIN);
        lv_obj_set_style_line_width(seg, 4, LV_PART_MAIN);
        lv_obj_set_style_line_rounded(seg, true, LV_PART_MAIN);
        lv_obj_set_style_line_opa(seg, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_clear_flag(seg, LV_OBJ_FLAG_CLICKABLE);
    }

    rpm_pointer = lv_line_create(root);
    rpm_pointer_points[0] = (lv_point_t){81, 166};
    rpm_pointer_points[1] = (lv_point_t){81, 181};
    lv_line_set_points(rpm_pointer, rpm_pointer_points, 2);
    lv_obj_set_style_line_width(rpm_pointer, 5, LV_PART_MAIN);
    lv_obj_set_style_line_rounded(rpm_pointer, true, LV_PART_MAIN);
    lv_obj_set_style_line_color(rpm_pointer, lv_color_white(), LV_PART_MAIN);
    lv_obj_add_flag(rpm_pointer, LV_OBJ_FLAG_HIDDEN);

    /* Transparent compatibility widgets for existing mock_data.cpp semantics. */
    objects.container_gauge = lv_obj_create(root);
    lv_obj_set_pos(objects.container_gauge, 0, 0);
    lv_obj_set_size(objects.container_gauge, 800, 480);
    lv_obj_set_style_bg_opa(objects.container_gauge, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(objects.container_gauge, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(objects.container_gauge, 0, LV_PART_MAIN);
    lv_obj_clear_flag(objects.container_gauge, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    objects.label_rpm = make_label(objects.container_gauge, "0000", 197, RPM_LABEL_Y, 406,
                                  &ui_font_km_136, 0xF5F6FA);
    objects.arc_rpm_0_40 = lv_arc_create(root);
    lv_arc_set_range(objects.arc_rpm_0_40, 0, 4000);
    lv_obj_add_flag(objects.arc_rpm_0_40, LV_OBJ_FLAG_HIDDEN);
    objects.bar_rpm_40_80 = lv_bar_create(root);
    lv_bar_set_range(objects.bar_rpm_40_80, 0, 4000);
    lv_obj_add_flag(objects.bar_rpm_40_80, LV_OBJ_FLAG_HIDDEN);

    objects.container_throttle = lv_obj_create(root);
    lv_obj_set_pos(objects.container_throttle, 0, 0);
    lv_obj_set_size(objects.container_throttle, 800, 480);
    lv_obj_set_style_bg_opa(objects.container_throttle, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(objects.container_throttle, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(objects.container_throttle, 0, LV_PART_MAIN);
    lv_obj_clear_flag(objects.container_throttle, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    objects.label_batt = make_label(objects.container_throttle, "--.- V", 164, 340, 172,
                                    &ui_font_Hollow85, 0xFFFFFF);
    objects.label_throttle = make_label(objects.container_throttle, "--.- C", 551, 340, 185,
                                        &ui_font_Hollow85, 0xFFFFFF);
    battery_value = objects.label_batt;
    temp_value = objects.label_throttle;
    objects.label_lambda = make_label(objects.container_throttle, "", 0, 0, 1,
                                      &ui_font_Hollow85, 0xFFFFFF);
    lv_obj_add_flag(objects.label_lambda, LV_OBJ_FLAG_HIDDEN);

    /* Two low-profile visible controls on the bottom carbon area.
       They do not cover the battery/temperature panels. */
    make_touch_button(root, 12, 1, "MODE 1", "DASH", 0x22DFFF);
    make_touch_button(root, 244, 2, "NIXIE", "MODE 2", 0x22DFFF);
    make_touch_button(root, 410, 3, "ECU DATA", "MODE 3", 0xFF3752);
    make_touch_button(root, 642, 4, "MODE 2", "NIXIE", 0xFF9A35);

}

void tick_screen_main(void) {}
void change_color_theme(uint32_t theme_index) {
    active_theme_index = theme_index;
    if (objects.main) lv_obj_invalidate(objects.main);
}
typedef void (*tick_screen_func_t)(void);
tick_screen_func_t tick_screen_funcs[] = {tick_screen_main};
void tick_screen(int screen_index) { if (screen_index == 0) tick_screen_main(); }
void tick_screen_by_id(enum ScreensEnum screenId) { if ((int)screenId == 1) tick_screen_main(); }
void create_screens(void) {
    lv_disp_t *d = lv_disp_get_default();
    lv_theme_t *t = lv_theme_default_init(d, lv_palette_main(LV_PALETTE_RED),
                                           lv_palette_main(LV_PALETTE_GREY), true, LV_FONT_DEFAULT);
    lv_disp_set_theme(d, t);
    create_screen_main();
}
int get_marker_right_state(void) { return TEST_MODE ? 1 : 0; }
