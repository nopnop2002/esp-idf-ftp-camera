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
	ESP_LOGI(pcTaskGetName(NULL), "Start");
	FTP_t ftpBuf;
	while(1) {
		xQueueReceive(xQueueFtp, &ftpBuf, portMAX_DELAY);
		ESP_LOGI(pcTaskGetName(NULL),"ftpBuf.command=%d", ftpBuf.command);
		if (ftpBuf.command == CMD_HALT) break;

		// Open FTP server
		ESP_LOGI(pcTaskGetName(NULL), "ftp server :%s", CONFIG_FTP_SERVER);
		ESP_LOGI(pcTaskGetName(NULL), "ftp user   :%s", CONFIG_FTP_USER);
		ESP_LOGI(pcTaskGetName(NULL), "ftp port   :%d", CONFIG_FTP_PORT);
		ESP_LOGI(pcTaskGetName(NULL), "localFile  :%s", ftpBuf.localFileName);
		ESP_LOGI(pcTaskGetName(NULL), "remoteFile :%s", ftpBuf.remoteFileName);
		ESP_LOGI(pcTaskGetName(NULL), "remoteDir  :%s", ftpBuf.remoteDirName);
		ESP_LOGI(pcTaskGetName(NULL), "taskHandle :%d", (unsigned int)ftpBuf.taskHandle);
		static NetBuf_t* ftpClientNetBuf = NULL;
		FtpClient* ftpClient = getFtpClient();
		//int connect = ftpClient->ftpClientConnect(CONFIG_FTP_SERVER, 21, &ftpClientNetBuf);
		int connect = ftpClient->ftpClientConnect(CONFIG_FTP_SERVER, CONFIG_FTP_PORT, &ftpClientNetBuf);
		if (connect == 0) {
			ESP_LOGE(pcTaskGetName(NULL), "FTP server connect fail");
			continue;
		}

		// Login FTP server
		int login = ftpClient->ftpClientLogin(CONFIG_FTP_USER, CONFIG_FTP_PASSWORD, ftpClientNetBuf);
		if (login == 0) {
			ESP_LOGE(pcTaskGetName(NULL), "FTP server login fail");
			continue;
		}

		// Create a directory
		if (strlen(ftpBuf.remoteDirName)) {
			//int mkdir = ftpClient->ftpClientMakeDir("pictire", ftpClientNetBuf);
			int mkdir = ftpClient->ftpClientMakeDir(ftpBuf.remoteDirName, ftpClientNetBuf);
			ESP_LOGD(pcTaskGetName(NULL), "mkdir=%d", mkdir);

			// Change working directory
			//int chdir = ftpClient->ftpClientChangeDir("pictire", ftpClientNetBuf);
			int chdir = ftpClient->ftpClientChangeDir(ftpBuf.remoteDirName, ftpClientNetBuf);
			ESP_LOGD(pcTaskGetName(NULL), "chdir=%d", chdir);
			if (chdir == 0) {
				ESP_LOGE(pcTaskGetName(NULL), "FTP server chdir fail");
				continue;
			}
		}

		// Put Picture to FTP server
		int put = ftpClient->ftpClientPut(ftpBuf.localFileName, ftpBuf.remoteFileName, FTP_CLIENT_BINARY, ftpClientNetBuf);
		if (put == 0) {
			ESP_LOGE(pcTaskGetName(NULL), "FTP server put fail");
			continue;
		}

		if (strlen(ftpBuf.remoteDirName)) {
			ESP_LOGI(pcTaskGetName(NULL), "ftpClientPut %s ---> %s/%s", ftpBuf.localFileName, ftpBuf.remoteDirName, ftpBuf.remoteFileName);
		} else {
			ESP_LOGI(pcTaskGetName(NULL), "ftpClientPut %s ---> %s", ftpBuf.localFileName, ftpBuf.remoteFileName);
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

// Build new directory
esp_err_t ftp_make_dir(char * path, FtpClient* ftpClient, NetBuf_t* ftpClientNetBuf)
{
	ESP_LOGI(__FUNCTION__, "path=[%s]",path);
	char *start = path;
	char *find;
	while(1) {
		ESP_LOGD(__FUNCTION__, "start=%p [%s]", start, start);
		find = strstr(start, "/");
		ESP_LOGD(__FUNCTION__, "find=%p", find);
		if (find == 0) break;
		ESP_LOGD(__FUNCTION__, "find=%p [%s]", find, find);

		char newpath[64];
		memset(newpath, 0, sizeof(newpath));
		strncpy(newpath, path, find-path);
		ESP_LOGI(__FUNCTION__, "newpath=[%s]",newpath);
		int mkdir = ftpClient->ftpClientMakeDir(newpath, ftpClientNetBuf);
		ESP_LOGI(__FUNCTION__, "mkdir=%d", mkdir);
		start = find + 1;
	}
	return ESP_OK;
}

void ftp_get(void *pvParameters)
{
	ESP_LOGI(pcTaskGetName(NULL), "Start");
	ESP_LOGI(pcTaskGetName(NULL), "CONFIG_FTP_SERVER      :%s", CONFIG_FTP_SERVER);
	ESP_LOGI(pcTaskGetName(NULL), "CONFIG_FTP_USER        :%s", CONFIG_FTP_USER);
	ESP_LOGI(pcTaskGetName(NULL), "CONFIG_FTP_PORT        :%d", CONFIG_FTP_PORT);
	ESP_LOGI(pcTaskGetName(NULL), "CONFIG_REMOTE_FILE_NAME:%s", CONFIG_REMOTE_FILE_NAME);

	// Open FTP server
	static NetBuf_t* ftpClientNetBuf = NULL;
	FtpClient* ftpClient = getFtpClient();
	//int connect = ftpClient->ftpClientConnect(CONFIG_FTP_SERVER, 21, &ftpClientNetBuf);
	int connect = ftpClient->ftpClientConnect(CONFIG_FTP_SERVER, CONFIG_FTP_PORT, &ftpClientNetBuf);
	if (connect == 0) {
		ESP_LOGE(pcTaskGetName(NULL), "FTP server connect fail");
		vTaskDelete( NULL );
	}

	// Login FTP server
	int login = ftpClient->ftpClientLogin(CONFIG_FTP_USER, CONFIG_FTP_PASSWORD, ftpClientNetBuf);
	if (login == 0) {
		ESP_LOGE(pcTaskGetName(NULL), "FTP server login fail");
		vTaskDelete( NULL );
	}

	static NetBuf_t* ftpClientNetData = NULL;
#if CONFIG_REMOTE_FILE_REMOVE
	ftp_make_dir(CONFIG_REMOTE_FILE_NAME, ftpClient, ftpClientNetBuf);
	int access = ftpClient->ftpClientAccess(CONFIG_REMOTE_FILE_NAME, FTP_CLIENT_FILE_WRITE, FTP_CLIENT_ASCII, ftpClientNetBuf, &ftpClientNetData);
	ESP_LOGI(pcTaskGetName(NULL), "ftpClientAccess=%d", access);
	if (access == 1) {
		char buffer[10];
		int write = ftpClient->ftpClientWrite(buffer, 0, ftpClientNetData);
		ESP_LOGI(pcTaskGetName(NULL), "ftpClientwrite=%d", write);
		ftpClient->ftpClientClose(ftpClientNetData);
	} else {
		ESP_LOGE(pcTaskGetName(NULL), "FTP server access fail");
		vTaskDelete( NULL );
	}
#endif // CONFIG_REMOTE_FILE_REMOVE

	CMD_t cmdBuf;
	cmdBuf.taskHandle = xTaskGetCurrentTaskHandle();
	cmdBuf.command = CMD_TAKE;

	while(1) {
		// Open file from FTP server
		int get = ftpClient->ftpClientAccess(CONFIG_REMOTE_FILE_NAME, FTP_CLIENT_FILE_READ, FTP_CLIENT_ASCII, ftpClientNetBuf, &ftpClientNetData);
		ESP_LOGD(pcTaskGetName(NULL), "ftpClientGet=%d", get);
		if (get == 1) {
			int close = ftpClient->ftpClientClose(ftpClientNetData);
			ESP_LOGD(pcTaskGetName(NULL), "ftpClientClose %d", close);
		}
#if CONFIG_REMOTE_FILE_CREATE
		// Triggered on remote file creation
		if (get == 1) {
			ESP_LOGI(pcTaskGetName(NULL), "remote file create %s", CONFIG_REMOTE_FILE_NAME);
			int delete = ftpClient->ftpClientDelete(CONFIG_REMOTE_FILE_NAME, ftpClientNetBuf);
			ESP_LOGI(pcTaskGetName(NULL), "ftpClientDelete %d", delete);
			if (xQueueSend(xQueueCmd, &cmdBuf, 10) != pdPASS) {
				ESP_LOGE(pcTaskGetName(NULL), "xQueueSend fail");
			}
		}
#endif // CONFIG_REMOTE_FILE_CREATE

#if CONFIG_REMOTE_FILE_REMOVE
		// Trigger on remote file deletion
		if (get == 0) {
			ESP_LOGI(pcTaskGetName(NULL), "remote file delete %s", CONFIG_REMOTE_FILE_NAME);
			if (xQueueSend(xQueueCmd, &cmdBuf, 10) != pdPASS) {
				ESP_LOGE(pcTaskGetName(NULL), "xQueueSend fail");
			}
			access = ftpClient->ftpClientAccess(CONFIG_REMOTE_FILE_NAME, FTP_CLIENT_FILE_WRITE, FTP_CLIENT_ASCII, ftpClientNetBuf, &ftpClientNetData);
			ESP_LOGI(pcTaskGetName(NULL), "ftpClientAccess=%d", access);
			if (access == 1) {
				char buffer[10];
				int write = ftpClient->ftpClientWrite(buffer, 0, ftpClientNetData);
				ESP_LOGI(pcTaskGetName(NULL), "ftpClientwrite=%d", write);
				ftpClient->ftpClientClose(ftpClientNetData);
			} else {
				ESP_LOGE(pcTaskGetName(NULL), "FTP server access fail");
			}
		}
#endif // CONFIG_REMOTE_FILE_REMOVE
		vTaskDelay(100);

	} // end while

	/* Never reach */
	vTaskDelete( NULL );
}
#endif // CONFIG_SHUTTER_REMOTE_FILE

