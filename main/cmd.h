#define CMD_TAKE	100
#define CMD_HALT	300
#define CMD_FTP		500

typedef struct {
	uint16_t command;
	TaskHandle_t taskHandle;
} CMD_t;

// Message to FTP Client
typedef struct {
	char localFileName[64];
	char remoteFileName[64];
	uint16_t command;
	TaskHandle_t taskHandle;
} FTP_t;

// Message to HTTP Server
typedef struct {
    char localFileName[64];
    TaskHandle_t taskHandle;
} HTTP_t;
