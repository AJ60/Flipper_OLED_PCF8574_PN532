#include "parse/flip_weather_parse.h"

bool sent_get_request = false;
bool get_request_success = false;
bool got_ip_address = false;
bool geo_information_processed = false;
bool weather_information_processed = false;

bool send_geo_location_request()
{
    if (fhttp.state == INACTIVE)
    {
        FURI_LOG_E(TAG, "Board is INACTIVE");
        flipper_http_ping(); // ping the device
        fhttp.state = ISSUE;
        return false;
    }
    if (!flipper_http_get_request_with_headers("https://ipwhois.app/json/", "{\"Content-Type\": \"application/json\"}"))
    {
        FURI_LOG_E(TAG, "Failed to send GET request");
        fhttp.state = ISSUE;
        return false;
    }
    fhttp.state = RECEIVING;
    return true;
}

bool send_geo_weather_request(DataLoaderModel *model)
{
    UNUSED(model);
    char url[512];
    char *lattitude = lat_data + 10;
    char *longitude = lon_data + 11;
    snprintf(url, 512, "https://api.open-meteo.com/v1/forecast?latitude=%s&longitude=%s&current=temperature_2m,precipitation,rain,showers,snowfall&temperature_unit=celsius&wind_speed_unit=mph&precipitation_unit=inch&forecast_days=1", lattitude, longitude);
    if (!flipper_http_get_request_with_headers(url, "{\"Content-Type\": \"application/json\"}"))
    {
        FURI_LOG_E(TAG, "Failed to send GET request");
        fhttp.state = ISSUE;
        return false;
    }
    fhttp.state = RECEIVING;
    return true;
}
char *process_geo_location(DataLoaderModel *model)
{
    UNUSED(model);
    if (fhttp.last_response != NULL)
    {
        char *city = get_json_value("city", fhttp.last_response, MAX_TOKENS);
        char *region = get_json_value("region", fhttp.last_response, MAX_TOKENS);
        char *country = get_json_value("country", fhttp.last_response, MAX_TOKENS);
        char *latitude = get_json_value("latitude", fhttp.last_response, MAX_TOKENS);
        char *longitude = get_json_value("longitude", fhttp.last_response, MAX_TOKENS);

        if (city == NULL || region == NULL || country == NULL || latitude == NULL || longitude == NULL)
        {
            FURI_LOG_E(TAG, "Failed to get geo location data");
            fhttp.state = ISSUE;
            return NULL;
        }

        snprintf(lat_data, sizeof(lat_data), "Latitude: %s", latitude);
        snprintf(lon_data, sizeof(lon_data), "Longitude: %s", longitude);

        if (!total_data)
        {
            total_data = (char *)malloc(512);
            if (!total_data)
            {
                FURI_LOG_E(TAG, "Failed to allocate memory for total_data");
                fhttp.state = ISSUE;
                return NULL;
            }
        }
        snprintf(total_data, 512, "You are in %s, %s, %s. \nLatitude: %s, Longitude: %s", city, region, country, latitude, longitude);

        fhttp.state = IDLE;
        free(city);
        free(region);
        free(country);
        free(latitude);
        free(longitude);
    }
    return total_data;
}

bool process_geo_location_2()
{
    if (fhttp.last_response != NULL)
    {
        char *city = get_json_value("city", fhttp.last_response, MAX_TOKENS);
        char *region = get_json_value("region", fhttp.last_response, MAX_TOKENS);
        char *country = get_json_value("country", fhttp.last_response, MAX_TOKENS);
        char *latitude = get_json_value("latitude", fhttp.last_response, MAX_TOKENS);
        char *longitude = get_json_value("longitude", fhttp.last_response, MAX_TOKENS);

        if (city == NULL || region == NULL || country == NULL || latitude == NULL || longitude == NULL)
        {
            FURI_LOG_E(TAG, "Failed to get geo location data");
            fhttp.state = ISSUE;
            return false;
        }

        snprintf(lat_data, sizeof(lat_data), "Latitude: %s", latitude);
        snprintf(lon_data, sizeof(lon_data), "Longitude: %s", longitude);

        fhttp.state = IDLE;
        free(city);
        free(region);
        free(country);
        free(latitude);
        free(longitude);
        return true;
    }
    return false;
}

char *process_weather(DataLoaderModel *model)
{
    UNUSED(model);
    if (fhttp.last_response != NULL)
    {
        char *current_data = get_json_value("current", fhttp.last_response, MAX_TOKENS);
        char *temperature = get_json_value("temperature_2m", current_data, MAX_TOKENS);
        char *precipitation = get_json_value("precipitation", current_data, MAX_TOKENS);
        char *rain = get_json_value("rain", current_data, MAX_TOKENS);
        char *showers = get_json_value("showers", current_data, MAX_TOKENS);
        char *snowfall = get_json_value("snowfall", current_data, MAX_TOKENS);
        char *time = get_json_value("time", current_data, MAX_TOKENS);

        if (current_data == NULL || temperature == NULL || precipitation == NULL || rain == NULL || showers == NULL || snowfall == NULL || time == NULL)
        {
            FURI_LOG_E(TAG, "Failed to get weather data");
            fhttp.state = ISSUE;
            return NULL;
        }

        // replace the "T" in time with a space
        char *ptr = strstr(time, "T");
        if (ptr != NULL)
        {
            *ptr = ' ';
        }

        if (!weather_data)
        {
            weather_data = (char *)malloc(512);
            if (!weather_data)
            {
                FURI_LOG_E(TAG, "Failed to allocate memory for weather_data");
                fhttp.state = ISSUE;
                return NULL;
            }
        }
        snprintf(weather_data, 512, "Temperature: %s C\nPrecipitation: %s\nRain: %s\nShowers: %s\nSnowfall: %s\nTime: %s", temperature, precipitation, rain, showers, snowfall, time);

        fhttp.state = IDLE;
        free(current_data);
        free(temperature);
        free(precipitation);
        free(rain);
        free(showers);
        free(snowfall);
        free(time);
    }
    return weather_data;
}