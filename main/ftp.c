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

extern QueueHandle_t xQueueFtp;

void ftp_put(void *pvParameters)
{
	ESP_LOGI(pcTaskGetName(0), "Start");
	FTP_t ftpBuf;
	while(1) {
		xQueueReceive(xQueueFtp, &ftpBuf, portMAX_DELAY);
		ESP_LOGI(pcTaskGetName(0),"ftpBuf.command=%d", ftpBuf.command);
		if (ftpBuf.command == CMD_HALT) break;

		// Open FTP server
		ESP_LOGI(pcTaskGetName(0), "ftp server :%s", CONFIG_FTP_SERVER);
		ESP_LOGI(pcTaskGetName(0), "ftp user   :%s", CONFIG_FTP_USER);
		ESP_LOGI(pcTaskGetName(0), "localFile  :%s", ftpBuf.localFileName);
		ESP_LOGI(pcTaskGetName(0), "remoteFile :%s", ftpBuf.remoteFileName);
		ESP_LOGI(pcTaskGetName(0), "remoteDir  :%s", ftpBuf.remoteDirName);
		ESP_LOGI(pcTaskGetName(0), "taskHandle :%d", ftpBuf.taskHandle);
		static NetBuf_t* ftpClientNetBuf = NULL;
		FtpClient* ftpClient = getFtpClient();
		int connect = ftpClient->ftpClientConnect(CONFIG_FTP_SERVER, 21, &ftpClientNetBuf);
		//int connect = ftpClient->ftpClientConnect(CONFIG_FTP_SERVER, 2121, &ftpClientNetBuf);
		if (connect == 0) {
			ESP_LOGE(pcTaskGetName(0), "FTP server connect fail");
			continue;
		}

		// Login FTP server
		int login = ftpClient->ftpClientLogin(CONFIG_FTP_USER, CONFIG_FTP_PASSWORD, ftpClientNetBuf);
		if (login == 0) {
			ESP_LOGE(pcTaskGetName(0), "FTP server login fail");
			continue;
		}

		// Create a directory
		if (strlen(ftpBuf.remoteDirName)) {
			//int mkdir = ftpClient->ftpClientMakeDir("pictire", ftpClientNetBuf);
			int mkdir = ftpClient->ftpClientMakeDir(ftpBuf.remoteDirName, ftpClientNetBuf);
			ESP_LOGD(pcTaskGetName(0), "mkdir=%d", mkdir);

			// Change working directory
			//int chdir = ftpClient->ftpClientChangeDir("pictire", ftpClientNetBuf);
			int chdir = ftpClient->ftpClientChangeDir(ftpBuf.remoteDirName, ftpClientNetBuf);
			ESP_LOGD(pcTaskGetName(0), "chdir=%d", chdir);
			if (chdir == 0) {
				ESP_LOGE(pcTaskGetName(0), "FTP server chdir fail");
				continue;
			}
		}

		// Put Picture to FTP server
		int put = ftpClient->ftpClientPut(ftpBuf.localFileName, ftpBuf.remoteFileName, FTP_CLIENT_BINARY, ftpClientNetBuf);
		if (put == 0) {
			ESP_LOGE(pcTaskGetName(0), "FTP server put fail");
			continue;
		}

		if (strlen(ftpBuf.remoteDirName)) {
			ESP_LOGI(pcTaskGetName(0), "ftpClientPut %s ---> %s/%s", ftpBuf.localFileName, ftpBuf.remoteDirName, ftpBuf.remoteFileName);
		} else {
			ESP_LOGI(pcTaskGetName(0), "ftpClientPut %s ---> %s", ftpBuf.localFileName, ftpBuf.remoteFileName);
		}

		ftpClient->ftpClientQuit(ftpClientNetBuf);

		// Notify the end of processing
		xTaskNotifyGive( ftpBuf.taskHandle );

	} // end while

	/* Never reach */
	vTaskDelete( NULL );

}

#if CONFIG_SHUTTER_REMOTE_FILE
extern QueueHandle_t xQueueCmd;

void ftp_get(void *pvParameters)
{
	char *base_path = (char *)pvParameters;
	ESP_LOGI(pcTaskGetName(0), "Start. base_path=%s", base_path);

	// Open FTP server
	ESP_LOGI(pcTaskGetName(0), "ftp server :%s", CONFIG_FTP_SERVER);
	ESP_LOGI(pcTaskGetName(0), "ftp user   :%s", CONFIG_FTP_USER);
	static NetBuf_t* ftpClientNetBuf = NULL;
	FtpClient* ftpClient = getFtpClient();
	int connect = ftpClient->ftpClientConnect(CONFIG_FTP_SERVER, 21, &ftpClientNetBuf);
	//int connect = ftpClient->ftpClientConnect(CONFIG_FTP_SERVER, 2121, &ftpClientNetBuf);
	if (connect == 0) {
		ESP_LOGE(pcTaskGetName(0), "FTP server connect fail");
		vTaskDelete( NULL );
	}

	// Login FTP server
	int login = ftpClient->ftpClientLogin(CONFIG_FTP_USER, CONFIG_FTP_PASSWORD, ftpClientNetBuf);
	if (login == 0) {
		ESP_LOGE(pcTaskGetName(0), "FTP server login fail");
		vTaskDelete( NULL );
	}

#if CONFIG_REMOTE_FILE_REMOVE
	static NetBuf_t* ftpClientNetData = NULL;
	int access;
	access = ftpClient->ftpClientAccess(CONFIG_REMOTE_FILE_NAME, FTP_CLIENT_FILE_WRITE, FTP_CLIENT_ASCII, ftpClientNetBuf, &ftpClientNetData);
	ESP_LOGI(pcTaskGetName(0), "ftpClientAccess=%d", access);
	if (access == 1) {
		char buffer[10];
		int write = ftpClient->ftpClientWrite(buffer, 0, ftpClientNetData);
		ESP_LOGI(pcTaskGetName(0), "ftpClientwrite=%d", write);
		ftpClient->ftpClientClose(ftpClientNetData);
	} else {
		ESP_LOGE(pcTaskGetName(0), "FTP server access fail");
		vTaskDelete( NULL );
	}
#endif // CONFIG_REMOTE_FILE_REMOVE

	CMD_t cmdBuf;
	cmdBuf.taskHandle = xTaskGetCurrentTaskHandle();
	cmdBuf.command = CMD_TAKE;
	char remoteFileName[64];
	char localFileName[64];
	//sprintf(remoteFileName, "shutter.txt");
	sprintf(remoteFileName, CONFIG_REMOTE_FILE_NAME);
	sprintf(localFileName, "%s/shutter.txt", base_path);
	ESP_LOGI(pcTaskGetName(0), "remoteFileName=%s", remoteFileName);
	ESP_LOGI(pcTaskGetName(0), "localFileName=%s", localFileName);

	while(1) {
		// Get file from FTP server
		int get = ftpClient->ftpClientGet(localFileName, remoteFileName, FTP_CLIENT_TEXT, ftpClientNetBuf);
		ESP_LOGD(pcTaskGetName(0), "ftpClientGet %d", get);
#if CONFIG_REMOTE_FILE_CREATE
		// Triggered on remote file creation
		if (get == 1) {
			ESP_LOGI(pcTaskGetName(0), "ftpClientGet %s <--- %s", localFileName, remoteFileName);
			int delete = ftpClient->ftpClientDelete(remoteFileName, ftpClientNetBuf);
			ESP_LOGI(pcTaskGetName(0), "ftpClientDelete %d", delete);
			if (xQueueSend(xQueueCmd, &cmdBuf, 10) != pdPASS) {
				ESP_LOGE(pcTaskGetName(0), "xQueueSend fail");
			}
		}
#endif
#if CONFIG_REMOTE_FILE_REMOVE
		// Trigger on remote file deletion
		if (get == 0) {
			ESP_LOGI(pcTaskGetName(0), "remote file removed %s", remoteFileName);
			if (xQueueSend(xQueueCmd, &cmdBuf, 10) != pdPASS) {
				ESP_LOGE(pcTaskGetName(0), "xQueueSend fail");
			}
			access = ftpClient->ftpClientAccess(CONFIG_REMOTE_FILE_NAME, FTP_CLIENT_FILE_WRITE, FTP_CLIENT_ASCII, ftpClientNetBuf, &ftpClientNetData);
			ESP_LOGI(pcTaskGetName(0), "ftpClientAccess=%d", access);
			if (access == 1) {
				char buffer[10];
				int write = ftpClient->ftpClientWrite(buffer, 0, ftpClientNetData);
				ESP_LOGI(pcTaskGetName(0), "ftpClientwrite=%d", write);
				ftpClient->ftpClientClose(ftpClientNetData);
			} else {
				ESP_LOGE(pcTaskGetName(0), "FTP server access fail");
			}
		}
#endif
		vTaskDelay(100);

	} // end while

	/* Never reach */
	vTaskDelete( NULL );
}
#endif // CONFIG_SHUTTER_REMOTE_FILE

