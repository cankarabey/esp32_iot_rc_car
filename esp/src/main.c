#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "cJSON.h"
#include "esp_http_client.h"
#include "driver/mcpwm.h"
#include "soc/mcpwm_periph.h"
#include "driver/gpio.h"

#define SERVO_MIN_PULSEWIDTH 1000
#define SERVO_MAX_PULSEWIDTH 2000
#define SERVO_MAX_DEGREE 180

#define GPIO_NUM_18 18
#define GPIO_NUM_19 19
#define GPIO_NUM_23 23
#define GPIO_NUM_32 32

const char *ssid = "";
const char *pass = "";
int retry_num=0;

int motor_action = 0;
int steering_angle = 0;
int motor_speed = 0;

static void wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id,void *event_data){
if(event_id == WIFI_EVENT_STA_START)
{
  printf("WIFI CONNECTING....\n");
}
else if (event_id == WIFI_EVENT_STA_CONNECTED)
{
  printf("WiFi CONNECTED\n");
}
else if (event_id == WIFI_EVENT_STA_DISCONNECTED)
{
  printf("WiFi lost connection\n");
  if(retry_num<5){esp_wifi_connect();retry_num++;printf("Retrying to Connect...\n");}
}
else if (event_id == IP_EVENT_STA_GOT_IP)
{
  printf("Wifi got IP...\n\n");
}
}

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            break;
        case HTTP_EVENT_ON_CONNECTED:
            break;
        case HTTP_EVENT_HEADER_SENT:
            break;
        case HTTP_EVENT_ON_HEADER:
            break;
        case HTTP_EVENT_ON_DATA:
            if (!esp_http_client_is_chunked_response(evt->client)) {
                cJSON *json = cJSON_ParseWithLength(evt->data, evt->data_len);
                if (json == NULL) {
                    ESP_LOGE("JSON", "JSON parsing failed");
                    break;
                }

                cJSON *json_motor_action = cJSON_GetObjectItemCaseSensitive(json, "motor_action");
                if (cJSON_IsNumber(json_motor_action)) {
                    motor_action = json_motor_action->valueint;
                }

                cJSON *json_steering_angle = cJSON_GetObjectItemCaseSensitive(json, "steering_angle");
                if (cJSON_IsNumber(json_steering_angle)) {
                    steering_angle = json_steering_angle->valueint;
                }

                cJSON *json_motor_speed = cJSON_GetObjectItemCaseSensitive(json, "motor_speed");
                if (cJSON_IsNumber(json_motor_speed)) {
                    motor_speed = json_motor_speed->valueint;
                }

                cJSON_Delete(json);
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            break;
        case HTTP_EVENT_DISCONNECTED:
            break;
        case HTTP_EVENT_REDIRECT:
            break;
    }
    return ESP_OK;
}

void wifi_connection()
{
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t wifi_initiation = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wifi_initiation);    
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);
    wifi_config_t wifi_configuration = {
        .sta = {
            .ssid = "",
            .password = "",
            
           }
    
        };
    strcpy((char*)wifi_configuration.sta.ssid, ssid);
    strcpy((char*)wifi_configuration.sta.password, pass);    
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_configuration);
    esp_wifi_start();
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_connect();
    printf( "wifi_init_softap finished. SSID:%s  password:%s",ssid,pass);
    
}

void http_get_task(void *pvParameters)
{
    esp_http_client_config_t config = {
        .url = "http://192.168.0.117:5000/status",
        .event_handler = _http_event_handler,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_perform(client);

    esp_http_client_cleanup(client);
    vTaskDelete(NULL);
}

void mcpwm_example_gpio_initialize(void)
{
    printf("initializing mcpwm control gpio......\n");
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, GPIO_NUM_18);
    mcpwm_gpio_init(MCPWM_UNIT_1, MCPWM1A, GPIO_NUM_32);
}

void motor_speed_control(int speed) {
    uint32_t duty_cycle = (speed * 1000) / 100;
    mcpwm_set_duty(MCPWM_UNIT_1, MCPWM_TIMER_1, MCPWM_OPR_A, duty_cycle);
    mcpwm_set_duty_type(MCPWM_UNIT_1, MCPWM_TIMER_1, MCPWM_OPR_A, MCPWM_DUTY_MODE_0);
}

uint32_t servo_per_degree_init(uint32_t degree_of_rotation)
{
    uint32_t cal_pulsewidth = 0;
    cal_pulsewidth = (SERVO_MIN_PULSEWIDTH + (((SERVO_MAX_PULSEWIDTH - SERVO_MIN_PULSEWIDTH) * (degree_of_rotation)) / (SERVO_MAX_DEGREE)));
    return cal_pulsewidth;
}

void servo_rotate(int angle){
    mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, servo_per_degree_init(angle));
};

void motor_forward(){
    gpio_set_level(GPIO_NUM_19, 0);
    gpio_set_level(GPIO_NUM_23, 1);
};

void motor_backward(){
    gpio_set_level(GPIO_NUM_19, 1);
    gpio_set_level(GPIO_NUM_23, 0);
};

void motor_stop(){
    gpio_set_level(GPIO_NUM_19, 0);
    gpio_set_level(GPIO_NUM_23, 0);
};

void app_main(void)
{
    mcpwm_example_gpio_initialize();
    printf("Configuring Initial Parameters of mcpwm......\n");
    mcpwm_config_t pwm_config;
    pwm_config.frequency = 50;
    pwm_config.cmpr_a = 0;
    pwm_config.cmpr_b = 0;
    pwm_config.counter_mode = MCPWM_UP_COUNTER;
    pwm_config.duty_mode = MCPWM_DUTY_MODE_0;
    mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &pwm_config);
    mcpwm_init(MCPWM_UNIT_1, MCPWM_TIMER_1, &pwm_config);

    gpio_reset_pin(GPIO_NUM_19);
    gpio_reset_pin(GPIO_NUM_23);

    gpio_set_direction(GPIO_NUM_19, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_NUM_23, GPIO_MODE_OUTPUT);

    gpio_set_level(GPIO_NUM_19, 0);
    gpio_set_level(GPIO_NUM_23, 0);

    nvs_flash_init();
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    wifi_connection();
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    esp_netif_init();
    esp_event_loop_create_default();
    while(1){
        xTaskCreate(&http_get_task, "http_get_task", 8192, NULL, 5, NULL);
        printf("%d \n", steering_angle);
        printf("%d \n", motor_action);
        printf("%d \n", motor_speed);
        servo_rotate(steering_angle);
        motor_speed_control(motor_speed);
        if(motor_action == 1){
            motor_forward();
        }
        else if(motor_action == 0){
            motor_stop();
        }
        else if(motor_action == -1){
            motor_backward();
        }
        else{
            motor_stop();
        }
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}