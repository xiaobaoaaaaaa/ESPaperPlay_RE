#ifndef EEZ_LVGL_UI_SCREENS_H
#define EEZ_LVGL_UI_SCREENS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _objects_t {
    lv_obj_t *main;
    lv_obj_t *menu;
    lv_obj_t *weather;
    lv_obj_t *weather_daily;
    lv_obj_t *settings;
    lv_obj_t *obj0;
    lv_obj_t *obj0__obj0;
    lv_obj_t *obj0__wifi_signal;
    lv_obj_t *obj0__obj1;
    lv_obj_t *obj1;
    lv_obj_t *obj1__obj0;
    lv_obj_t *obj1__wifi_signal;
    lv_obj_t *obj1__obj1;
    lv_obj_t *obj2;
    lv_obj_t *obj3;
    lv_obj_t *obj4;
    lv_obj_t *obj5;
    lv_obj_t *obj6;
    lv_obj_t *obj7;
    lv_obj_t *obj8;
    lv_obj_t *obj9;
    lv_obj_t *obj10;
    lv_obj_t *main_page_weather_icon;
    lv_obj_t *main_page_weather_temp;
    lv_obj_t *main_page_weather_uptime;
    lv_obj_t *main_page_weather_text;
    lv_obj_t *obj11;
    lv_obj_t *main_page_lunar;
    lv_obj_t *obj12;
    lv_obj_t *obj13;
    lv_obj_t *obj14;
    lv_obj_t *obj15;
    lv_obj_t *obj16;
    lv_obj_t *obj17;
    lv_obj_t *obj18;
    lv_obj_t *obj19;
    lv_obj_t *obj20;
    lv_obj_t *obj21;
    lv_obj_t *obj22;
    lv_obj_t *obj23;
    lv_obj_t *obj24;
    lv_obj_t *obj25;
    lv_obj_t *obj26;
    lv_obj_t *obj27;
    lv_obj_t *obj28;
    lv_obj_t *obj29;
    lv_obj_t *obj30;
    lv_obj_t *obj31;
    lv_obj_t *obj32;
    lv_obj_t *obj33;
    lv_obj_t *obj34;
    lv_obj_t *obj35;
    lv_obj_t *obj36;
    lv_obj_t *obj37;
    lv_obj_t *obj38;
    lv_obj_t *obj39;
    lv_obj_t *obj40;
    lv_obj_t *obj41;
    lv_obj_t *obj42;
    lv_obj_t *obj43;
    lv_obj_t *obj44;
    lv_obj_t *weather_daily_temp_min1;
    lv_obj_t *weather_daily_icon1;
    lv_obj_t *weather_daily_date1;
    lv_obj_t *weather_daily_temp_bar1;
    lv_obj_t *weather_daily_temp_max1;
    lv_obj_t *obj45;
    lv_obj_t *weather_daily_temp_min2;
    lv_obj_t *weather_daily_icon2;
    lv_obj_t *weather_daily_date2;
    lv_obj_t *weather_daily_temp_bar2;
    lv_obj_t *weather_daily_temp_max2;
    lv_obj_t *obj46;
    lv_obj_t *weather_daily_temp_min3;
    lv_obj_t *weather_daily_icon3;
    lv_obj_t *weather_daily_date3;
    lv_obj_t *weather_daily_temp_bar3;
    lv_obj_t *weather_daily_temp_max3;
    lv_obj_t *obj47;
    lv_obj_t *weather_daily_temp_min4;
    lv_obj_t *weather_daily_icon4;
    lv_obj_t *weather_daily_date4;
    lv_obj_t *weather_daily_temp_bar4;
    lv_obj_t *weather_daily_temp_max4;
    lv_obj_t *obj48;
    lv_obj_t *weather_daily_temp_min5;
    lv_obj_t *weather_daily_icon5;
    lv_obj_t *weather_daily_date5;
    lv_obj_t *weather_daily_temp_bar5;
    lv_obj_t *weather_daily_temp_max5;
    lv_obj_t *obj49;
    lv_obj_t *weather_daily_temp_min6;
    lv_obj_t *weather_daily_icon6;
    lv_obj_t *weather_daily_date6;
    lv_obj_t *weather_daily_temp_bar6;
    lv_obj_t *weather_daily_temp_max6;
    lv_obj_t *obj50;
    lv_obj_t *weather_daily_temp_min7;
    lv_obj_t *weather_daily_icon7;
    lv_obj_t *weather_daily_date7;
    lv_obj_t *weather_daily_temp_bar7;
    lv_obj_t *weather_daily_temp_max7;
    lv_obj_t *obj51;
} objects_t;

extern objects_t objects;

enum ScreensEnum {
    SCREEN_ID_MAIN = 1,
    SCREEN_ID_MENU = 2,
    SCREEN_ID_WEATHER = 3,
    SCREEN_ID_WEATHER_DAILY = 4,
    SCREEN_ID_SETTINGS = 5,
};

void create_screen_main();
void delete_screen_main();
void tick_screen_main();

void create_screen_menu();
void delete_screen_menu();
void tick_screen_menu();

void create_screen_weather();
void delete_screen_weather();
void tick_screen_weather();

void create_screen_weather_daily();
void delete_screen_weather_daily();
void tick_screen_weather_daily();

void create_screen_settings();
void delete_screen_settings();
void tick_screen_settings();

void create_user_widget_status_bar(lv_obj_t *parent_obj, void *flowState, int startWidgetIndex);
void tick_user_widget_status_bar(void *flowState, int startWidgetIndex);

void create_screen_by_id(enum ScreensEnum screenId);
void delete_screen_by_id(enum ScreensEnum screenId);
void tick_screen_by_id(enum ScreensEnum screenId);
void tick_screen(int screen_index);

void create_screens();


#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_SCREENS_H*/