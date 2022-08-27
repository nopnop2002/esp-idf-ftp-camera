/*
   FTP client example.
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_log.h"

#include "FtpClient.h"
#include "cmd.h"

/* The examples use WiFi configuration that you can set via project configuration menu

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define ESP_WIFI_SSID "mywifissid"
*/
#define ESP_FTP_SERVER			CONFIG_FTP_SERVER
#define ESP_FTP_USER			CONFIG_FTP_USER  
#define ESP_FTP_PASSWORD		CONFIG_FTP_PASSWORD

extern QueueHandle_t xQueueFtp;

static const char *TAG = "FTP";

void ftp(void *pvParameters)
{
	TaskHandle_t taskHandle = (TaskHandle_t)pvParameters;
	ESP_LOGI(TAG, "Start. taskHandle=%d", taskHandle);
	FTP_t ftpBuf;
	while(1) {
		xQueueReceive(xQueueFtp, &ftpBuf, portMAX_DELAY);
		ESP_LOGI(TAG,"ftpBuf.command=%d", ftpBuf.command);
		if (ftpBuf.command == CMD_HALT) break;

		// Open FTP server
		ESP_LOGI(TAG, "ftp server :%s", ESP_FTP_SERVER);
		ESP_LOGI(TAG, "ftp user   :%s", ESP_FTP_USER);
		ESP_LOGI(TAG, "localFile  :%s", ftpBuf.localFileName);
		ESP_LOGI(TAG, "remoteFile :%s", ftpBuf.remoteFileName);
		static NetBuf_t* ftpClientNetBuf = NULL;
		FtpClient* ftpClient = getFtpClient();
		int connect = ftpClient->ftpClientConnect(ESP_FTP_SERVER, 21, &ftpClientNetBuf);
		if (connect == 0) {
			ESP_LOGE(TAG, "FTP server connect fail");
			continue;
		}

		// Login FTP server
		int login = ftpClient->ftpClientLogin(ESP_FTP_USER, ESP_FTP_PASSWORD, ftpClientNetBuf);
		if (login == 0) {
			ESP_LOGE(TAG, "FTP server login fail");
			continue;
		}

#if CONFIG_ENABLE_SUBDIR
		// Create a directory
		//int mkdir = ftpClient->ftpClientMakeDir("pictire", ftpClientNetBuf);
		int mkdir = ftpClient->ftpClientMakeDir(CONFIG_FTP_SUBDIR, ftpClientNetBuf);
		ESP_LOGD(TAG, "mkdir=%d", mkdir);

		// Change working directory
		//int chdir = ftpClient->ftpClientChangeDir("pictire", ftpClientNetBuf);
		int chdir = ftpClient->ftpClientChangeDir(CONFIG_FTP_SUBDIR, ftpClientNetBuf);
		ESP_LOGD(TAG, "chdir=%d", chdir);
		if (chdir == 0) {
			ESP_LOGE(TAG, "FTP server chdir fail");
			continue;
		}
#endif

		// Put Picture to FTP server
		int put = ftpClient->ftpClientPut(ftpBuf.localFileName, ftpBuf.remoteFileName, FTP_CLIENT_BINARY, ftpClientNetBuf);
		if (put == 0) {
			ESP_LOGE(TAG, "FTP server put fail");
			continue;
		}
#if CONFIG_ENABLE_SUBDIR
		ESP_LOGI(TAG, "ftpClientPut %s ---> %s/%s", ftpBuf.localFileName, CONFIG_FTP_SUBDIR, ftpBuf.remoteFileName);
#else
		ESP_LOGI(TAG, "ftpClientPut %s ---> %s", ftpBuf.localFileName, ftpBuf.remoteFileName);
#endif

#if 0
		// Delete Local file
		unlink(ftpBuf.localFileName);
		ESP_LOGI(TAG, "Local file removed");
#endif

		ftpClient->ftpClientQuit(ftpClientNetBuf);

		// Notify the end of processing
		xTaskNotifyGive( taskHandle );

	} // end while

	/* Never reach */
	vTaskDelete( NULL );

}

