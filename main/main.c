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
#include "lwip/dns.h"

#include "esp_camera.h"

#include "cmd.h"


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

#define BOARD_ESP32CAM_AITHINKER

// WROVER-KIT PIN Map
#ifdef BOARD_WROVER_KIT

#define CAM_PIN_PWDN -1  //power down is not used
#define CAM_PIN_RESET -1 //software reset will be performed
#define CAM_PIN_XCLK 21
#define CAM_PIN_SIOD 26
#define CAM_PIN_SIOC 27

#define CAM_PIN_D7 35
#define CAM_PIN_D6 34
#define CAM_PIN_D5 39
#define CAM_PIN_D4 36
#define CAM_PIN_D3 19
#define CAM_PIN_D2 18
#define CAM_PIN_D1 5
#define CAM_PIN_D0 4
#define CAM_PIN_VSYNC 25
#define CAM_PIN_HREF 23
#define CAM_PIN_PCLK 22

#endif

// ESP32Cam (AiThinker) PIN Map
#ifdef BOARD_ESP32CAM_AITHINKER

#define CAM_PIN_PWDN 32
#define CAM_PIN_RESET -1 //software reset will be performed
#define CAM_PIN_XCLK 0
#define CAM_PIN_SIOD 26
#define CAM_PIN_SIOC 27

#define CAM_PIN_D7 35
#define CAM_PIN_D6 34
#define CAM_PIN_D5 39
#define CAM_PIN_D4 36
#define CAM_PIN_D3 21
#define CAM_PIN_D2 19
#define CAM_PIN_D1 18
#define CAM_PIN_D0 5
#define CAM_PIN_VSYNC 25
#define CAM_PIN_HREF 23
#define CAM_PIN_PCLK 22

#endif


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
	.frame_size = FRAMESIZE_VGA,	//QQVGA-UXGA Do not use sizes above QVGA when not JPEG

	.jpeg_quality = 12, //0-63 lower number means higher quality
	.fb_count = 1		//if more than one, i2s runs in continuous mode. Use only with JPEG
};

static esp_err_t init_camera(int framesize)
{
	ESP_LOGI(TAG, "Start init_camera");
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

static void event_handler(void* arg, esp_event_base_t event_base,
								int32_t event_id, void* event_data)
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


void wifi_init_sta()
{
	s_wifi_event_group = xEventGroupCreate();

	ESP_LOGI(TAG,"ESP-IDF esp_netif");
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
	esp_netif_set_ip_info(netif, &ip_info);

	/*
	I referred from here.
	https://www.esp32.com/viewtopic.php?t=5380

	if we should not be using DHCP (for example we are using static IP addresses),
	then we need to instruct the ESP32 of the locations of the DNS servers manually.
	Google publicly makes available two name servers with the addresses of 8.8.8.8 and 8.8.4.4.
	*/

	ip_addr_t d;
	d.type = IPADDR_TYPE_V4;
	d.u_addr.ip4.addr = 0x08080808; //8.8.8.8 dns
	dns_setserver(0, &d);
	d.u_addr.ip4.addr = 0x08080404; //8.8.4.4 dns
	dns_setserver(1, &d);

#endif // CONFIG_STATIC_IP

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

	wifi_config_t wifi_config = {
		.sta = {
			.ssid = CONFIG_ESP_WIFI_SSID,
			.password = CONFIG_ESP_WIFI_PASSWORD
		},
	};
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
	ESP_ERROR_CHECK(esp_wifi_start() );

	ESP_LOGI(TAG, "wifi_init_sta finished.");

	/* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
	 * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
	EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
			WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
			pdFALSE,
			pdFALSE,
			portMAX_DELAY);

	/* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
	 * happened. */
	if (bits & WIFI_CONNECTED_BIT) {
		ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
				 CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD);
	} else if (bits & WIFI_FAIL_BIT) {
		ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
				 CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD);
	} else {
		ESP_LOGE(TAG, "UNEXPECTED EVENT");
	}
	vEventGroupDelete(s_wifi_event_group);
}

void initialise_mdns(void)
{
	//initialize mDNS
	ESP_ERROR_CHECK( mdns_init() );
	//set mDNS hostname (required if you want to advertise services)
	ESP_ERROR_CHECK( mdns_hostname_set(CONFIG_MDNS_HOSTNAME) );
	ESP_LOGI(TAG, "mdns hostname set to: [%s]", CONFIG_MDNS_HOSTNAME);

#if 0
	//set default mDNS instance name
	ESP_ERROR_CHECK( mdns_instance_name_set("ESP32 with mDNS") );
#endif
}

esp_err_t mountSPIFFS(char * partition_label, char * base_path) {
	ESP_LOGI(TAG, "Initializing SPIFFS file system");

	esp_vfs_spiffs_conf_t conf = {
		.base_path = base_path,
		.partition_label = partition_label,
		.max_files = 5,
		.format_if_mount_failed = true
	};

	// Use settings defined above to initialize and mount SPIFFS filesystem.
	// Note: esp_vfs_spiffs_register is an all-in-one convenience function.
	esp_err_t ret = esp_vfs_spiffs_register(&conf);

	if (ret != ESP_OK) {
		if (ret == ESP_FAIL) {
			ESP_LOGE(TAG, "Failed to mount or format filesystem");
		} else if (ret == ESP_ERR_NOT_FOUND) {
			ESP_LOGE(TAG, "Failed to find SPIFFS partition");
		} else {
			ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
		}
		return ret;
	}

	size_t total = 0, used = 0;
	ret = esp_spiffs_info(partition_label, &total, &used);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
	} else {
		ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
	}
	ESP_LOGI(TAG, "Mount SPIFFS filesystem");
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

void ftp_put(void *pvParameters);

#if CONFIG_SHUTTER_ENTER
void keyin(void *pvParameters);
#endif

#if CONFIG_SHUTTER_GPIO
void gpio(void *pvParameters);
#endif

#if CONFIG_SHUTTER_TCP
void tcp_server(void *pvParameters);
#endif

#if CONFIG_SHUTTER_UDP
void udp_server(void *pvParameters);
#endif

#if CONFIG_SHUTTER_HTTP
void web_server(void *pvParameters);
#endif

#if CONFIG_SHUTTER_REMOTE_FILE
void ftp_get(void *pvParameters);
#endif

void http_task(void *pvParameters);

void app_main()
{
	//Initialize NVS
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

	ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
	wifi_init_sta();
	initialise_mdns();

#if CONFIG_REMOTE_IS_VARIABLE_NAME
	// obtain time over NTP
	ESP_LOGI(TAG, "Connecting to WiFi and getting time over NTP.");
	ret = obtain_time();
	if(ret != ESP_OK) {
		ESP_LOGE(TAG, "Fail to getting time over NTP.");
		return;
	}

	// update 'now' variable with current time
	time_t now;
	struct tm timeinfo;
	char strftime_buf[64];
	time(&now);
	now = now + (CONFIG_LOCAL_TIMEZONE*60*60);
	localtime_r(&now, &timeinfo);
	strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
	ESP_LOGI(TAG, "The current date/time is: %s", strftime_buf);
#endif // CONFIG_REMOTE_IS_VARIABLE_NAME

	char *partition_label = "storage0";
	char *base_path = "/spiffs"; 
	ret = mountSPIFFS(partition_label, base_path);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "mountSPIFFS fail");
		while(1) { vTaskDelay(1); }
	}

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
	xTaskCreate(web_server, "WEB", 1024*4, NULL, 2, NULL);
#endif

#if CONFIG_SHUTTER_REMOTE_FILE
#define SHUTTER "REMOTE file polling"
	xTaskCreate(ftp_get, "FTP_GET", 1024*8, NULL, 2, NULL);
#endif

	/* Get the local IP address */
	//tcpip_adapter_ip_info_t ip_info;
	//ESP_ERROR_CHECK(tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info));
	esp_netif_ip_info_t ip_info;
	ESP_ERROR_CHECK(esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &ip_info));

	// It is not possible to start two HTTP servers
#if !CONFIG_SHUTTER_HTTP
	/* Create HTTP Task */
	char cparam0[64];
	//sprintf(cparam0, "%s", ip4addr_ntoa(&ip_info.ip));
	sprintf(cparam0, IPSTR, IP2STR(&ip_info.ip));
	ESP_LOGI(TAG, "cparam0=[%s]", cparam0);
	xTaskCreate(http_task, "HTTP", 1024*6, (void *)cparam0, 2, NULL);
#endif

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
	//sprintf(ftpBuf.remoteFileName, "picture.jpg");
#if CONFIG_REMOTE_FRAMESIZE
	sprintf(ftpBuf.remoteFileName, "%s_%s", CONFIG_FIXED_REMOTE_FILE, FRAMESIZE_STRING);
#else
	sprintf(ftpBuf.remoteFileName, "%s", CONFIG_FIXED_REMOTE_FILE);
#endif
	ESP_LOGI(TAG, "remoteFileName=%s",ftpBuf.remoteFileName);
#endif
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
		sprintf(ftpBuf.remoteFileName, "%04d%02d%02d-%02d%02d%02d_%s.jpg",
		(timeinfo.tm_year+1900),(timeinfo.tm_mon+1),timeinfo.tm_mday,
		timeinfo.tm_hour,timeinfo.tm_min,timeinfo.tm_sec, FRAMESIZE_STRING);
#else
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

#if !CONFIG_SHUTTER_HTTP
		// send local file name to http task
		if (xQueueSend(xQueueHttp, &httpBuf, 10) != pdPASS) {
			ESP_LOGE(TAG, "xQueueSend xQueueHttp fail");
		}
#endif

	} // end while

	esp_vfs_spiffs_unregister(NULL);
	ESP_LOGI(TAG, "SPIFFS unmounted");

}
