/*
	 Take a picture and Publish it via FTP.

	 This code is in the Public Domain (or CC0 licensed, at your option.)

	 Unless required by applicable law or agreed to in writing, this
	 software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
	 CONDITIONS OF ANY KIND, either express or implied.

	 I ported from here:
	 https://github.com/espressif/esp32-camera/blob/master/examples/take_picture.c
*/

#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_spiffs.h" 
#include "esp_sntp.h"
#include "mdns.h"

#include "esp_camera.h"
#include "camera_pin.h"

#include "cmd.h"

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0))
#define sntp_setoperatingmode esp_sntp_setoperatingmode
#define sntp_setservername esp_sntp_setservername
#define sntp_init esp_sntp_init
#endif

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static const char *TAG = "MAIN";

static int s_retry_num = 0;

QueueHandle_t xQueueCmd;
QueueHandle_t xQueueFtp;
QueueHandle_t xQueueHttp;

//static camera_config_t camera_config = {
camera_config_t camera_config = {
	.pin_pwdn = CAM_PIN_PWDN,
	.pin_reset = CAM_PIN_RESET,
	.pin_xclk = CAM_PIN_XCLK,
	.pin_sscb_sda = CAM_PIN_SIOD,
	.pin_sscb_scl = CAM_PIN_SIOC,

	.pin_d7 = CAM_PIN_D7,
	.pin_d6 = CAM_PIN_D6,
	.pin_d5 = CAM_PIN_D5,
	.pin_d4 = CAM_PIN_D4,
	.pin_d3 = CAM_PIN_D3,
	.pin_d2 = CAM_PIN_D2,
	.pin_d1 = CAM_PIN_D1,
	.pin_d0 = CAM_PIN_D0,
	.pin_vsync = CAM_PIN_VSYNC,
	.pin_href = CAM_PIN_HREF,
	.pin_pclk = CAM_PIN_PCLK,

	//XCLK 20MHz or 10MHz for OV2640 double FPS (Experimental)
	.xclk_freq_hz = 20000000,
	.ledc_timer = LEDC_TIMER_0,
	.ledc_channel = LEDC_CHANNEL_0,

	.pixel_format = PIXFORMAT_JPEG, //YUV422,GRAYSCALE,RGB565,JPEG
	.frame_size = FRAMESIZE_VGA, //QQVGA-UXGA Do not use sizes above QVGA when not JPEG

	.jpeg_quality = 12, //0-63 lower number means higher quality
	.fb_count = 1, //if more than one, i2s runs in continuous mode. Use only with JPEG
	.grab_mode = CAMERA_GRAB_WHEN_EMPTY,
	.fb_location = CAMERA_FB_IN_PSRAM
};

static esp_err_t init_camera(int framesize)
{
	ESP_LOGI(TAG, "Start init_camera");
	ESP_LOGI(TAG, "grab_mode=%d", camera_config.grab_mode);
	ESP_LOGI(TAG, "fb_location=%d", camera_config.fb_location);
	//initialize the camera
	camera_config.frame_size = framesize;
	esp_err_t err = esp_camera_init(&camera_config);
	if (err != ESP_OK)
	{
		ESP_LOGE(TAG, "Camera Init Failed");
		return err;
	}

	ESP_LOGI(TAG, "Finish init_camera");
	return ESP_OK;
}

static esp_err_t camera_capture(char * FileName, size_t *pictureSize)
{
	//clear internal queue
	//for(int i=0;i<2;i++) {
	for(int i=0;i<1;i++) {
		camera_fb_t * fb = esp_camera_fb_get();
		ESP_LOGI(TAG, "fb->len=%d", fb->len);
		esp_camera_fb_return(fb);
	}

	//acquire a frame
	camera_fb_t * fb = esp_camera_fb_get();
	if (!fb) {
		ESP_LOGE(TAG, "Camera Capture Failed");
		return ESP_FAIL;
	}

	//replace this with your own function
	//process_image(fb->width, fb->height, fb->format, fb->buf, fb->len);
	FILE* f = fopen(FileName, "wb");
	if (f == NULL) {
		ESP_LOGE(TAG, "Failed to open file for writing");
		return ESP_FAIL; 
	}
	fwrite(fb->buf, fb->len, 1, f);
	ESP_LOGI(TAG, "fb->len=%d", fb->len);
	*pictureSize = (size_t)fb->len;
	fclose(f);
	
	//return the frame buffer back to the driver for reuse
	esp_camera_fb_return(fb);

	return ESP_OK;
}

static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
		esp_wifi_connect();
	} else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
		if (s_retry_num < CONFIG_ESP_MAXIMUM_RETRY) {
			esp_wifi_connect();
			s_retry_num++;
			ESP_LOGI(TAG, "retry to connect to the AP");
		} else {
			xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
		}
		ESP_LOGI(TAG,"connect to the AP fail");
	} else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
		ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
		ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
		s_retry_num = 0;
		xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
	}
}

#if CONFIG_STATIC_IP
static esp_err_t example_set_dns_server(esp_netif_t *netif, uint32_t addr, esp_netif_dns_type_t type)
{
	if (addr && (addr != IPADDR_NONE)) {
		esp_netif_dns_info_t dns;
		dns.ip.u_addr.ip4.addr = addr;
		dns.ip.type = IPADDR_TYPE_V4;
		ESP_ERROR_CHECK(esp_netif_set_dns_info(netif, type, &dns));
	}
	return ESP_OK;
}
#endif

esp_err_t wifi_init_sta()
{
	s_wifi_event_group = xEventGroupCreate();

	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	esp_netif_t *netif = esp_netif_create_default_wifi_sta();
	assert(netif);

#if CONFIG_STATIC_IP

	ESP_LOGI(TAG, "CONFIG_STATIC_IP_ADDRESS=[%s]",CONFIG_STATIC_IP_ADDRESS);
	ESP_LOGI(TAG, "CONFIG_STATIC_GW_ADDRESS=[%s]",CONFIG_STATIC_GW_ADDRESS);
	ESP_LOGI(TAG, "CONFIG_STATIC_NM_ADDRESS=[%s]",CONFIG_STATIC_NM_ADDRESS);

	/* Stop DHCP client */
	ESP_ERROR_CHECK(esp_netif_dhcpc_stop(netif));
	ESP_LOGI(TAG, "Stop DHCP Services");

	/* Set STATIC IP Address */
	esp_netif_ip_info_t ip_info;
	memset(&ip_info, 0 , sizeof(esp_netif_ip_info_t));
	ip_info.ip.addr = ipaddr_addr(CONFIG_STATIC_IP_ADDRESS);
	ip_info.netmask.addr = ipaddr_addr(CONFIG_STATIC_NM_ADDRESS);
	ip_info.gw.addr = ipaddr_addr(CONFIG_STATIC_GW_ADDRESS);;
	ESP_ERROR_CHECK(esp_netif_set_ip_info(netif, &ip_info));

	/* Set DNS Server */
	ESP_ERROR_CHECK(example_set_dns_server(netif, ipaddr_addr("8.8.8.8"), ESP_NETIF_DNS_MAIN));
	ESP_ERROR_CHECK(example_set_dns_server(netif, ipaddr_addr("8.8.4.4"), ESP_NETIF_DNS_BACKUP));

#endif // CONFIG_STATIC_IP

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	esp_event_handler_instance_t instance_any_id;
	esp_event_handler_instance_t instance_got_ip;
	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
		ESP_EVENT_ANY_ID,
		&event_handler,
		NULL,
		&instance_any_id));
	ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
		IP_EVENT_STA_GOT_IP,
		&event_handler,
		NULL,
		&instance_got_ip));

	wifi_config_t wifi_config = {
		.sta = {
			.ssid = CONFIG_ESP_WIFI_SSID,
			.password = CONFIG_ESP_WIFI_PASSWORD
		},
	};
	ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
	ESP_ERROR_CHECK(esp_wifi_start());


	/* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
	 * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
	esp_err_t ret_value = ESP_OK;
	EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
			WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
			pdFALSE,
			pdFALSE,
			portMAX_DELAY);

	/* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
	 * happened. */
	if (bits & WIFI_CONNECTED_BIT) {
		ESP_LOGI(TAG, "connected to ap SSID:%s password:%s", CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD);
	} else if (bits & WIFI_FAIL_BIT) {
		ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s", CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD);
		ret_value = ESP_FAIL;
	} else {
		ESP_LOGE(TAG, "UNEXPECTED EVENT");
		ret_value = ESP_FAIL;
	}

	/* The event will not be processed after unregister */
	ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
	ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
	vEventGroupDelete(s_wifi_event_group);
	return ret_value;
}

void initialize_mdns(void)
{
	//initialize mDNS
	ESP_ERROR_CHECK( mdns_init() );
	//set mDNS hostname (required if you want to advertise services)
	ESP_ERROR_CHECK( mdns_hostname_set(CONFIG_MDNS_HOSTNAME) );
	ESP_LOGI(TAG, "mdns hostname set to: [%s]", CONFIG_MDNS_HOSTNAME);

	//initialize service
	ESP_ERROR_CHECK( mdns_service_add("ESP32-WebServer", "_http", "_tcp", 8080, NULL, 0) );

#if 0
	//set default mDNS instance name
	ESP_ERROR_CHECK( mdns_instance_name_set("ESP32 with mDNS") );
#endif
}

esp_err_t mountSPIFFS(char * path, char * label, int max_files) {
	esp_vfs_spiffs_conf_t conf = {
		.base_path = path,
		.partition_label = label,
		.max_files = max_files,
		.format_if_mount_failed = true
	};

	// Use settings defined above to initialize and mount SPIFFS filesystem.
	// Note: esp_vfs_spiffs_register is an all-in-one convenience function.
	esp_err_t ret = esp_vfs_spiffs_register(&conf);

	if (ret != ESP_OK) {
		if (ret ==ESP_FAIL) {
			ESP_LOGE(TAG, "Failed to mount or format filesystem");
		} else if (ret== ESP_ERR_NOT_FOUND) {
			ESP_LOGE(TAG, "Failed to find SPIFFS partition");
		} else {
			ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)",esp_err_to_name(ret));
		}
		return ret;
	}

#if 0
	ESP_LOGI(TAG, "Performing SPIFFS_check().");
	ret = esp_spiffs_check(conf.partition_label);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "SPIFFS_check() failed (%s)", esp_err_to_name(ret));
		return ret;
	} else {
			ESP_LOGI(TAG, "SPIFFS_check() successful");
	}
#endif

	size_t total = 0, used = 0;
	ret = esp_spiffs_info(conf.partition_label, &total, &used);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG,"Failed to get SPIFFS partition information (%s)",esp_err_to_name(ret));
	} else {
		ESP_LOGI(TAG,"Mount %s to %s success", path, label);
		ESP_LOGI(TAG,"Partition size: total: %d, used: %d", total, used);
	}

	return ret;
}

#if CONFIG_REMOTE_IS_VARIABLE_NAME
void time_sync_notification_cb(struct timeval *tv)
{
	ESP_LOGI(TAG, "Notification of a time synchronization event");
}

static void initialize_sntp(void)
{
	ESP_LOGI(TAG, "Initializing SNTP");
	sntp_setoperatingmode(SNTP_OPMODE_POLL);
	//sntp_setservername(0, "pool.ntp.org");
	ESP_LOGI(TAG, "Your NTP Server is %s", CONFIG_NTP_SERVER);
	sntp_setservername(0, CONFIG_NTP_SERVER);
	sntp_set_time_sync_notification_cb(time_sync_notification_cb);
	sntp_init();
}

static esp_err_t obtain_time(void)
{
	initialize_sntp();
	// wait for time to be set
	int retry = 0;
	const int retry_count = 10;
	while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
		ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
		vTaskDelay(2000 / portTICK_PERIOD_MS);
	}

	if (retry == retry_count) return ESP_FAIL;
	return ESP_OK;
}
#endif // CONFIG_REMOTE_IS_VARIABLE_NAME

#if CONFIG_SHUTTER_MQTT
esp_err_t query_mdns_host(const char * host_name, char *ip)
{
	ESP_LOGD(__FUNCTION__, "Query A: %s", host_name);

	struct esp_ip4_addr addr;
	addr.addr = 0;

	esp_err_t err = mdns_query_a(host_name, 10000,	&addr);
	if(err){
		if(err == ESP_ERR_NOT_FOUND){
			ESP_LOGW(__FUNCTION__, "%s: Host was not found!", esp_err_to_name(err));
			return ESP_FAIL;
		}
		ESP_LOGE(__FUNCTION__, "Query Failed: %s", esp_err_to_name(err));
		return ESP_FAIL;
	}

	ESP_LOGD(__FUNCTION__, "Query A: %s.local resolved to: " IPSTR, host_name, IP2STR(&addr));
	sprintf(ip, IPSTR, IP2STR(&addr));
	return ESP_OK;
}

void convert_mdns_host(char * from, char * to)
{
	ESP_LOGI(__FUNCTION__, "from=[%s]",from);
	strcpy(to, from);
	char *sp;
	sp = strstr(from, ".local");
	if (sp == NULL) return;

	int _len = sp - from;
	ESP_LOGD(__FUNCTION__, "_len=%d", _len);
	char _from[128];
	strcpy(_from, from);
	_from[_len] = 0;
	ESP_LOGI(__FUNCTION__, "_from=[%s]", _from);

	char _ip[128];
	esp_err_t ret = query_mdns_host(_from, _ip);
	ESP_LOGI(__FUNCTION__, "query_mdns_host=%d _ip=[%s]", ret, _ip);
	if (ret != ESP_OK) return;

	strcpy(to, _ip);
	ESP_LOGI(__FUNCTION__, "to=[%s]", to);
}

#endif

void ftp_put(void *pvParameters);

#if CONFIG_SHUTTER_ENTER
void keyin(void *pvParameters);
#elif CONFIG_SHUTTER_GPIO
void gpio(void *pvParameters);
#elif CONFIG_SHUTTER_TCP
void tcp_server(void *pvParameters);
#elif CONFIG_SHUTTER_UDP
void udp_server(void *pvParameters);
#elif CONFIG_SHUTTER_MQTT
void mqtt_sub(void *pvParameters);
#elif CONFIG_SHUTTER_REMOTE_FILE
void ftp_get(void *pvParameters);
#endif

void http_task(void *pvParameters);

void app_main()
{
	// Initialize NVS
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

	// Initilize WiFi
	wifi_init_sta();

	// Initialize mDNS
	initialize_mdns();

#if CONFIG_REMOTE_IS_VARIABLE_NAME
	// obtain time over NTP
	ESP_LOGI(TAG, "Connecting to WiFi and getting time over NTP.");
	ret = obtain_time();
	if(ret != ESP_OK) {
		ESP_LOGE(TAG, "Fail to getting time over NTP.");
		return;
	}

	// show current date & time
	time_t now;
	struct tm timeinfo;
	char strftime_buf[64];
	time(&now);
	now = now + (CONFIG_LOCAL_TIMEZONE*60*60);
	localtime_r(&now, &timeinfo);
	strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
	ESP_LOGI(TAG, "The current date/time is: %s", strftime_buf);
#endif // CONFIG_REMOTE_IS_VARIABLE_NAME

	// Mount SPIFFS
	char *base_path = "/spiffs"; 
	ESP_ERROR_CHECK(mountSPIFFS(base_path, "storage", 2));

#if CONFIG_ENABLE_FLASH
	// Enable Flash Light
	//gpio_pad_select_gpio(CONFIG_GPIO_FLASH);
	gpio_reset_pin(CONFIG_GPIO_FLASH);
	gpio_set_direction(CONFIG_GPIO_FLASH, GPIO_MODE_OUTPUT);
	gpio_set_level(CONFIG_GPIO_FLASH, 0);
#endif

	/* Create Queue */
	xQueueCmd = xQueueCreate( 1, sizeof(CMD_t) );
	configASSERT( xQueueCmd );
	xQueueFtp = xQueueCreate( 1, sizeof(FTP_t) );
	configASSERT( xQueueFtp );
	xQueueHttp = xQueueCreate( 10, sizeof(HTTP_t) );
	configASSERT( xQueueHttp );

	/* Create FTP Task */
	xTaskCreate(ftp_put, "FTP_PUT", 1024*8, NULL, 2, NULL);

	/* Create Shutter Task */
#if CONFIG_SHUTTER_ENTER
#define SHUTTER "Keybord Enter"
	xTaskCreate(keyin, "KEYIN", 1024*4, NULL, 2, NULL);
#endif

#if CONFIG_SHUTTER_GPIO
#define SHUTTER "GPIO Input"
	xTaskCreate(gpio, "GPIO", 1024*4, NULL, 2, NULL);
#endif

#if CONFIG_SHUTTER_TCP
#define SHUTTER "TCP Input"
	xTaskCreate(tcp_server, "TCP", 1024*4, NULL, 2, NULL);
#endif

#if CONFIG_SHUTTER_UDP
#define SHUTTER "UDP Input"
	xTaskCreate(udp_server, "UDP", 1024*4, NULL, 2, NULL);
#endif

#if CONFIG_SHUTTER_HTTP
#define SHUTTER "HTTP Request"
#endif

#if CONFIG_SHUTTER_MQTT
#define SHUTTER "MQTT Subscrive"
	xTaskCreate(mqtt_sub, "SUB", 1024*4, NULL, 2, NULL);
#endif

#if CONFIG_SHUTTER_REMOTE_FILE
#define SHUTTER "REMOTE file polling"
	xTaskCreate(ftp_get, "FTP_GET", 1024*8, NULL, 2, NULL);
#endif

	/* Get the local IP address */
	esp_netif_ip_info_t ip_info;
	ESP_ERROR_CHECK(esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &ip_info));

	/* Create HTTP Task */
	char cparam0[64];
	sprintf(cparam0, IPSTR, IP2STR(&ip_info.ip));
	ESP_LOGI(TAG, "cparam0=[%s]", cparam0);
	xTaskCreate(http_task, "HTTP", 1024*6, (void *)cparam0, 2, NULL);

#if CONFIG_FRAMESIZE_VGA
	int framesize = FRAMESIZE_VGA;
	#define	FRAMESIZE_STRING "640x480"
#elif CONFIG_FRAMESIZE_SVGA
	int framesize = FRAMESIZE_SVGA;
	#define	FRAMESIZE_STRING "800x600"
#elif CONFIG_FRAMESIZE_XGA
	int framesize = FRAMESIZE_XGA;
	#define	FRAMESIZE_STRING "1024x768"
#elif CONFIG_FRAMESIZE_HD
	int framesize = FRAMESIZE_HD;
	#define	FRAMESIZE_STRING "1280x720"
#elif CONFIG_FRAMESIZE_SXGA
	int framesize = FRAMESIZE_SXGA;
	#define	FRAMESIZE_STRING "1280x1024"
#elif CONFIG_FRAMESIZE_UXGA
	int framesize = FRAMESIZE_UXGA;
	#define	FRAMESIZE_STRING "1600x1200"
#endif

	// Initialize camera
	ret = init_camera(framesize);
	if (ret != ESP_OK) {
		while(1) { vTaskDelay(1); }
	}

	FTP_t ftpBuf;
	ftpBuf.command = CMD_FTP;
	ftpBuf.taskHandle = xTaskGetCurrentTaskHandle();
	sprintf(ftpBuf.localFileName, "%s/picture.jpg", base_path);
	ESP_LOGI(TAG, "localFileName=%s",ftpBuf.localFileName);

#if CONFIG_REMOTE_IS_FIXED_NAME
#if CONFIG_REMOTE_FRAMESIZE
	char baseFileName[32];
	strcpy(baseFileName, CONFIG_FIXED_REMOTE_FILE);
	for (int index=0;index<strlen(baseFileName);index++) {
		if (baseFileName[index] == 0x2E) baseFileName[index] = 0;
	}
	ESP_LOGI(TAG, "baseFileName=[%s]", baseFileName);
	// picture_640x480.jpg
	sprintf(ftpBuf.remoteFileName, "%s_%s.jpg", baseFileName, FRAMESIZE_STRING);
#else
	// picture.jpg
	sprintf(ftpBuf.remoteFileName, "%s", CONFIG_FIXED_REMOTE_FILE);
#endif
	ESP_LOGI(TAG, "remoteFileName=%s",ftpBuf.remoteFileName);
#endif

	// Set remote file directory
	memset(ftpBuf.remoteDirName, 0, sizeof(ftpBuf.remoteDirName));
#if CONFIG_ENABLE_SUBDIR
	strcpy(ftpBuf.remoteDirName, CONFIG_FTP_SUBDIR);
#endif
		
	HTTP_t httpBuf;
	httpBuf.taskHandle = xTaskGetCurrentTaskHandle();
	strcpy(httpBuf.localFileName, ftpBuf.localFileName);
	
	CMD_t cmdBuf;
	while(1) {
		ESP_LOGI(TAG,"Waitting %s ....", SHUTTER);
		xQueueReceive(xQueueCmd, &cmdBuf, portMAX_DELAY);
		ESP_LOGI(TAG,"cmdBuf.command=%d", cmdBuf.command);
		if (cmdBuf.command == CMD_HALT) break;
		if (cmdBuf.command == CMD_RESTART) {
			ESP_LOGW(TAG, "Restarting after 10 seconds");
			vTaskDelay(1000);
			esp_restart();
		}

		// Delete local file
		struct stat statBuf;
		if (stat(ftpBuf.localFileName, &statBuf) == 0) {
			// Delete it if it exists
			unlink(ftpBuf.localFileName);
			ESP_LOGI(TAG, "Delete Local file");
		}

#if CONFIG_REMOTE_IS_VARIABLE_NAME
		time(&now);
		now = now + (CONFIG_LOCAL_TIMEZONE*60*60);
		localtime_r(&now, &timeinfo);
		strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
		ESP_LOGI(TAG, "The current date/time is: %s", strftime_buf);
#if CONFIG_REMOTE_FRAMESIZE
		// 20220927-110940_640x480.jpg
		sprintf(ftpBuf.remoteFileName, "%04d%02d%02d-%02d%02d%02d_%s.jpg",
		(timeinfo.tm_year+1900),(timeinfo.tm_mon+1),timeinfo.tm_mday,
		timeinfo.tm_hour,timeinfo.tm_min,timeinfo.tm_sec, FRAMESIZE_STRING);
#else
		// 20220927-110742.jpg
		sprintf(ftpBuf.remoteFileName, "%04d%02d%02d-%02d%02d%02d.jpg",
		(timeinfo.tm_year+1900),(timeinfo.tm_mon+1),timeinfo.tm_mday,
		timeinfo.tm_hour,timeinfo.tm_min,timeinfo.tm_sec);
#endif
		ESP_LOGI(TAG, "remoteFileName: %s", ftpBuf.remoteFileName);
#endif

#if CONFIG_ENABLE_FLASH
		// Flash Light ON
		gpio_set_level(CONFIG_GPIO_FLASH, 1);
#endif

		// Save Picture to Local file
		int retryCounter = 0;
		while(1) {
			size_t pictureSize;
			ret = camera_capture(ftpBuf.localFileName, &pictureSize);
			ESP_LOGI(TAG, "camera_capture=%d",ret);
			ESP_LOGI(TAG, "pictureSize=%d",pictureSize);
			if (ret != ESP_OK) continue;
			if (stat(ftpBuf.localFileName, &statBuf) == 0) {
				ESP_LOGI(TAG, "st_size=%d", (int)statBuf.st_size);
				if (statBuf.st_size == pictureSize) break;
				retryCounter++;
				ESP_LOGI(TAG, "Retry capture %d",retryCounter);
				if (retryCounter > 10) {
					ESP_LOGE(TAG, "Retry over for capture");
					break;
				}
				vTaskDelay(1000);
			}
		} // end while

#if CONFIG_ENABLE_FLASH
		// Flash Light OFF
		gpio_set_level(CONFIG_GPIO_FLASH, 0);
#endif

		// send picture via FTP
		ESP_LOGI(TAG, "xQueueSend ftpBuf");
		if (xQueueSend(xQueueFtp, &ftpBuf, 10) != pdPASS) {
			ESP_LOGE(TAG, "xQueueSend xQueueFtp fail");
		}

		// wait ftp complete
		ulTaskNotifyTake( pdTRUE, portMAX_DELAY );
		ESP_LOGI(TAG, "ulTaskNotifyTake");

#if CONFIG_CREATE_INDEX_HTML
		ESP_LOGI(TAG, "Opening index.html file");
		FTP_t indexBuf;
		indexBuf.command = CMD_FTP;
		indexBuf.taskHandle = xTaskGetCurrentTaskHandle();
		sprintf(indexBuf.localFileName, "%s/index.html", base_path);
		strcpy(indexBuf.remoteDirName, CONFIG_INDEX_HTML_SUBDIR);
		sprintf(indexBuf.remoteFileName, "index.html");
		char remoteFileName[128];
#if CONFIG_ENABLE_SUBDIR
		sprintf(remoteFileName, "%s/%s", CONFIG_FTP_SUBDIR, ftpBuf.remoteFileName);
#else
		sprintf(remoteFileName, "%s", ftpBuf.remoteFileName);
#endif

		FILE* f = fopen(indexBuf.localFileName, "w");
		if (f == NULL) {
			ESP_LOGE(TAG, "Failed to open file for writing");
		} else {
			ESP_LOGD(TAG, "ftpBuf.remoteFileName=%s", ftpBuf.remoteFileName);
			fprintf(f, "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\">\n");
			fprintf(f, "<html><head>\n");
			fprintf(f, "<meta http-equiv=\"content-type\" content=\"text/html; charset=UTF-8\">\n");
			fprintf(f, "<title>Picture</title>\n");
			fprintf(f, "</head><body>\n");
			fprintf(f, "%s\n", ftpBuf.remoteFileName);
#if CONFIG_IMAGE_ROTATE_0
			fprintf(f, "<img src=\"%s\" alt=\"%s\" style=\"transform:rotate(0deg);\">\n", remoteFileName, ftpBuf.remoteFileName);
#endif
#if CONFIG_IMAGE_ROTATE_90
			fprintf(f, "<img src=\"%s\" alt=\"%s\" style=\"transform:rotate(90deg);\">\n", remoteFileName, ftpBuf.remoteFileName);
#endif
#if CONFIG_IMAGE_ROTATE_180
			fprintf(f, "<img src=\"%s\" alt=\"%s\" style=\"transform:rotate(180deg);\">\n", remoteFileName, ftpBuf.remoteFileName);
#endif
#if CONFIG_IMAGE_ROTATE_270
			fprintf(f, "<img src=\"%s\" alt=\"%s\" style=\"transform:rotate(270deg);\">\n", remoteFileName, ftpBuf.remoteFileName);
#endif
			fprintf(f, "</body></html>\n");
			fclose(f);
			ESP_LOGI(TAG, "File written");

			// send index.html via FTP
			if (xQueueSend(xQueueFtp, &indexBuf, 10) != pdPASS) {
				ESP_LOGE(TAG, "xQueueSend xQueueFtp fail");
			}

			// wait ftp complete
			ulTaskNotifyTake( pdTRUE, portMAX_DELAY );
			ESP_LOGI(TAG, "ulTaskNotifyTake");
			ESP_LOGD(TAG, "Open this in your browser http://%s/%s/index.html", CONFIG_FTP_SERVER, CONFIG_INDEX_HTML_SUBDIR);
		}
#endif

		// send local file name to http task
		if (xQueueSend(xQueueHttp, &httpBuf, 10) != pdPASS) {
			ESP_LOGE(TAG, "xQueueSend xQueueHttp fail");
		}

	} // end while

	esp_vfs_spiffs_unregister(NULL);
	ESP_LOGI(TAG, "SPIFFS unmounted");

}
