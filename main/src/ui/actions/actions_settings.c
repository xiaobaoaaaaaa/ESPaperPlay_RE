#include "actions.h"
#include "fonts.h"
#include "screens.h"

void action_settings_page_load(lv_event_t *e) {
    lv_obj_t *screen = lv_event_get_target(e);
    lv_obj_t *menu = lv_menu_create(screen);
    lv_obj_set_pos(menu, 0, 15);
    lv_obj_set_size(menu, 200, 185);
    lv_menu_set_mode_root_back_button(menu, LV_MENU_ROOT_BACK_BUTTON_DISABLED);

    lv_obj_t *root_page = lv_menu_page_create(menu, NULL);
    lv_obj_t *display_page = lv_menu_page_create(menu, NULL);
    lv_obj_t *network_page = lv_menu_page_create(menu, NULL);
    lv_obj_t *about_page = lv_menu_page_create(menu, NULL);

    lv_obj_t *section = lv_menu_section_create(root_page);
    lv_obj_t *cont = lv_menu_cont_create(section);
    lv_obj_t *label = lv_label_create(cont);
    lv_obj_set_style_text_font(label, ui_font_source_han_sans_sc_14,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(label, "显示");
    lv_obj_set_style_text_color(cont, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(cont, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(cont, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(cont, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_CHECKED);
    lv_menu_set_load_page_event(menu, cont, display_page);

    cont = lv_menu_cont_create(section);
    label = lv_label_create(cont);
    lv_obj_set_style_text_font(label, ui_font_source_han_sans_sc_14,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(label, "网络");
    lv_obj_set_style_text_color(cont, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(cont, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(cont, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(cont, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_CHECKED);
    lv_menu_set_load_page_event(menu, cont, network_page);

    cont = lv_menu_cont_create(section);
    label = lv_label_create(cont);
    lv_obj_set_style_text_font(label, ui_font_source_han_sans_sc_14,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(label, "关于");
    lv_obj_set_style_text_color(cont, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(cont, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(cont, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(cont, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_CHECKED);
    lv_menu_set_load_page_event(menu, cont, about_page);

    section = lv_menu_section_create(display_page);
    cont = lv_menu_cont_create(section);
    label = lv_label_create(cont);
    lv_obj_set_style_text_font(label, ui_font_source_han_sans_sc_14,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(label, "显示设置");

    section = lv_menu_section_create(network_page);
    cont = lv_menu_cont_create(section);
    label = lv_label_create(cont);
    lv_obj_set_style_text_font(label, ui_font_source_han_sans_sc_14,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(label, "网络设置");

    section = lv_menu_section_create(about_page);
    cont = lv_menu_cont_create(section);
    label = lv_label_create(cont);
    lv_obj_set_style_text_font(label, ui_font_source_han_sans_sc_14,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(label, "设备信息");

    lv_menu_set_sidebar_page(menu, root_page);

    lv_menu_set_page(menu, display_page);
}
