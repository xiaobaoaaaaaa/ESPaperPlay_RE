#include "images.h"

const ext_img_desc_t images[6] = {
    { "weaher_pressure", &img_weaher_pressure },
    { "weather_precip", &img_weather_precip },
    { "weather_humidity", &img_weather_humidity },
    { "weather_visibility", &img_weather_visibility },
    { "weather_cloud", &img_weather_cloud },
    { "weather_dew", &img_weather_dew },
};
