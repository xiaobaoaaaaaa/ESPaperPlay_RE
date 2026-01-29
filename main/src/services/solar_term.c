#include <stdio.h>
#include <string.h>

// 24节气名称
const char *SOLAR_TERMS[] = {"小寒", "大寒", "立春", "雨水", "惊蛰", "春分", "清明", "谷雨",
                             "立夏", "小满", "芒种", "夏至", "小暑", "大暑", "立秋", "处暑",
                             "白露", "秋分", "寒露", "霜降", "立冬", "小雪", "大雪", "冬至"};

// 节气计算常量 (1900-2099)
// 每个月两个节气，数组索引 0-23 分别对应 小寒 到 冬至
const double TERM_C[] = {5.4055, 20.12,  3.87,  18.73,  5.63,  20.646, 4.81, 20.1,
                         5.52,   21.04,  5.678, 21.37,  7.108, 22.83,  7.5,  23.13,
                         7.646,  23.042, 8.318, 23.438, 7.438, 22.36,  7.18, 21.94};

// 判断是否为闰年
int is_leap(int year) { return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0); }

// 计算某年某月某节气在哪一天
int get_term_day(int year, int term_idx) {
    double y = (double)(year % 100);
    // 简化的寿星公式
    int day = (int)(y * 0.2422 + TERM_C[term_idx]) - (int)((y - 1) / 4);

    // 特殊年份修正
    if (year == 2019 && term_idx == 1)
        day -= 1; // 大寒修正

    return day;
}

/**
 * 核心函数：获取节气描述字符串
 * @param year  年
 * @param month 月
 * @param day   日
 * @param buffer 输出缓冲区（建议大小至少 32 字节）
 */
void get_solar_term_info(int year, int month, int day, char *buffer) {
    // 1. 获取本月两个节气
    int idx1 = (month - 1) * 2;
    int idx2 = (month - 1) * 2 + 1;
    int day1 = get_term_day(year, idx1);
    int day2 = get_term_day(year, idx2);

    int diff = 0;

    if (day == day1) {
        sprintf(buffer, "%s 当天", SOLAR_TERMS[idx1]);
    } else if (day == day2) {
        sprintf(buffer, "%s 当天", SOLAR_TERMS[idx2]);
    } else if (day < day1) {
        // 还没到本月第一个节气
        sprintf(buffer, "%s 距%d天", SOLAR_TERMS[idx1], day1 - day);
    } else if (day < day2) {
        // 过了第一个，还没到第二个
        // 逻辑：返回距离更近的那一个
        if ((day - day1) < (day2 - day)) {
            sprintf(buffer, "%s 已过%d天", SOLAR_TERMS[idx1], day - day1);
        } else {
            sprintf(buffer, "%s 距%d天", SOLAR_TERMS[idx2], day2 - day);
        }
    } else {
        // 过了本月第二个，计算下个月第一个节气
        int next_m = (month % 12) + 1;
        int next_y = (month == 12) ? year + 1 : year;
        int next_idx = (next_m - 1) * 2;
        int next_day1 = get_term_day(next_y, next_idx);

        // 跨月天数简单估算（假设本月30或31天）
        int days_in_month[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        if (is_leap(year))
            days_in_month[2] = 29;

        diff = (days_in_month[month] - day) + next_day1;
        sprintf(buffer, "%s 距%d天", SOLAR_TERMS[next_idx], diff);
    }
}