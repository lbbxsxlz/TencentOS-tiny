#include "tos_k.h"
#include "esp8266_tencent_firmware.h"
#include "tencent_firmware_module_wrapper.h"
#include "pm2d5_parser.h"
#include "oled.h"


#define PRODUCT_ID              "XOEHGW66ZD"
#define DEVICE_NAME             "pm0001"
#define DEVICE_KEY              "pVziOcDry+iOwcgP3kWCCw=="

#define REPORT_DATA_TEMPLATE    "{\\\"method\\\":\\\"report\\\"\\,\\\"clientToken\\\":\\\"00000001\\\"\\,\\\"params\\\":{\\\"Pm2d5Value\\\":%d}}"

void default_message_handler(mqtt_message_t* msg)
{
    printf("callback:\r\n");
    printf("---------------------------------------------------------\r\n");
    printf("\ttopic:%s\r\n", msg->topic);
    printf("\tpayload:%s\r\n", msg->payload);
    printf("---------------------------------------------------------\r\n");
}

char payload[256] = {0};
static char report_topic_name[TOPIC_NAME_MAX_SIZE] = {0};
static char report_reply_topic_name[TOPIC_NAME_MAX_SIZE] = {0};

k_mail_q_t mail_q;
pm2d5_data_u pm2d5_value;
uint8_t pm2d5_value_pool[3 * sizeof(pm2d5_data_u)];

void mqtt_demo_task(void)
{
    int ret = 0;
    int size = 0;
    mqtt_state_t state;
    int  i = 0;    
    char *product_id = PRODUCT_ID;
    char *device_name = DEVICE_NAME;
    char *key = DEVICE_KEY;
    
    device_info_t dev_info;
    memset(&dev_info, 0, sizeof(device_info_t));
    char str[16];   
    size_t mail_size;

    
    
    /* OLED��ʾ��־ */
    OLED_ShowString(0, 2, (uint8_t*)"connecting...", 16);

    /**
     * Please Choose your AT Port first, default is HAL_UART_2(USART2)
    */
    ret = esp8266_tencent_firmware_sal_init(HAL_UART_PORT_2);
    
    if (ret < 0) {
        printf("esp8266 tencent firmware sal init fail, ret is %d\r\n", ret);
    }
    
    esp8266_tencent_firmware_join_ap("Supowang", "13975426138");

    strncpy(dev_info.product_id, product_id, PRODUCT_ID_MAX_SIZE);
    strncpy(dev_info.device_name, device_name, DEVICE_NAME_MAX_SIZE);
    strncpy(dev_info.device_serc, key, DEVICE_SERC_MAX_SIZE);
    tos_tf_module_info_set(&dev_info, TLS_MODE_PSK);

    mqtt_param_t init_params = DEFAULT_MQTT_PARAMS;
    if (tos_tf_module_mqtt_conn(init_params) != 0) {
        printf("module mqtt conn fail\n");
    } else {
        printf("module mqtt conn success\n");
    }

    if (tos_tf_module_mqtt_state_get(&state) != -1) {
        printf("MQTT: %s\n", state == MQTT_STATE_CONNECTED ? "CONNECTED" : "DISCONNECTED");
    }
    
    /* ��ʼ����topic */
    size = snprintf(report_reply_topic_name, TOPIC_NAME_MAX_SIZE, "$thing/down/property/%s/%s", product_id, device_name);

    if (size < 0 || size > sizeof(report_reply_topic_name) - 1) {
        printf("sub topic content length not enough! content size:%d  buf size:%d", size, (int)sizeof(report_reply_topic_name));
    }
    if (tos_tf_module_mqtt_sub(report_reply_topic_name, QOS0, default_message_handler) != 0) {
        printf("module mqtt sub fail\n");
    } else {
        printf("module mqtt sub success\n");
    }
    
    memset(report_topic_name, sizeof(report_topic_name), 0);
    size = snprintf(report_topic_name, TOPIC_NAME_MAX_SIZE, "$thing/up/property/%s/%s", product_id, device_name);

    if (size < 0 || size > sizeof(report_topic_name) - 1) {
        printf("pub topic content length not enough! content size:%d  buf size:%d", size, (int)sizeof(report_topic_name));
    }
    
    /* �������� */
    tos_mail_q_create(&mail_q, pm2d5_value_pool, 3, sizeof(pm2d5_data_u));
    
    HAL_NVIC_DisableIRQ(USART3_4_IRQn);
    
    if (pm2d5_parser_init() == -1) {
        printf("pm2d5 parser init fail\r\n");
        return;
    }
  
    while (1) {
        /* ͨ�������ʼ�����ȡ���� */
        HAL_NVIC_EnableIRQ(USART3_4_IRQn);
        tos_mail_q_pend(&mail_q, (uint8_t*)&pm2d5_value, &mail_size, TOS_TIME_FOREVER);
        HAL_NVIC_DisableIRQ(USART3_4_IRQn);
        
        //�յ�֮���ӡ��Ϣ
        printf("\r\n\r\n\r\n");
        for (i = 0; i < 13; i++) {
            printf("data[%d]:%d ug/m3\r\n", i+1, pm2d5_value.data[i]);
        }
        
        //��ʾPM2.5��ֵ
        OLED_Clear();
        sprintf(str, "PM1.0:%4d ug/m3", pm2d5_value.pm2d5_data.data1);
        OLED_ShowString(0,0,(uint8_t*)str,16);
        sprintf(str, "PM2.5:%4d ug/m3", pm2d5_value.pm2d5_data.data2);
        OLED_ShowString(0,2,(uint8_t*)str,16);
				
        
        /* �ϱ�ֵ */
        memset(payload, 0, sizeof(payload));
        snprintf(payload, sizeof(payload), REPORT_DATA_TEMPLATE, pm2d5_value.pm2d5_data.data2);
        
        
        if (tos_tf_module_mqtt_pub(report_topic_name, QOS0, payload) != 0) {
            printf("module mqtt pub fail\n");
            break;
        } else {
            printf("module mqtt pub success\n");
        }
        
        tos_sleep_ms(5000);
    }
}

void application_entry(void *arg)
{
    char *str = "TencentOS-tiny";
    
    /* ��ʼ��OLED */
    OLED_Init();
    OLED_Clear();
    OLED_ShowString(0, 0, (uint8_t*)str, 16);
    
    mqtt_demo_task();
    while (1) {
        printf("This is a mqtt demo!\r\n");
        tos_task_delay(1000);
    }
}
