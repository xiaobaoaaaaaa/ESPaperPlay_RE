#include "images.h"

const ext_img_desc_t images[11] = {
    { "weaher_pressure", &img_weaher_pressure },
    { "weather_precip", &img_weather_precip },
    { "weather_humidity", &img_weather_humidity },
    { "weather_visibility", &img_weather_visibility },
    { "weather_cloud", &img_weather_cloud },
    { "weather_dew", &img_weather_dew },
    { "signal_1", &img_signal_1 },
    { "signal_2", &img_signal_2 },
    { "signal_3", &img_signal_3 },
    { "signal_4", &img_signal_4 },
    { "signal_0", &img_signal_0 },
};
