# esp-idf-ftp-camera
Take a picture and Publish it via FTP.   
This project use [ESP32 Camera Driver](https://github.com/espressif/esp32-camera).   
This project use [ESP32-FTP-Client](https://github.com/JohnnyB1290/ESP32-FTP-Client).   

![slide1](https://user-images.githubusercontent.com/6020549/119622002-b6906480-be41-11eb-84cd-d4a53532ee5e.JPG)
![slide2](https://user-images.githubusercontent.com/6020549/119622017-b98b5500-be41-11eb-8ad6-1bde963e7849.JPG)

# Hardware requirements
ESP32-CAM Development board.   
Support for OV2640 camera.   
If you use other camera, edit sdkconfig.default.   
![ESP32-CAM-1](https://user-images.githubusercontent.com/6020549/118466947-4fd2c300-b73e-11eb-8e64-23260e73e693.JPG)
![ESP32-CAM-2](https://user-images.githubusercontent.com/6020549/118466960-53664a00-b73e-11eb-8950-a5058516e1a3.JPG)


# Software requirements
esp-idf v4.4 or later.   
The mDNS strict mode [issue](https://github.com/espressif/esp-idf/issues/6190) has been resolved.   

# Installation
Use a USB-TTL converter.   

|ESP-32|USB-TTL|
|:-:|:-:|
|U0TXD|RXD|
|U0RXD|TXD|
|GPIO0|GND|
|5V|5V|
|GND|GND|


```
git clone https://github.com/nopnop2002/esp-idf-ftp-camera
cd esp-idf-ftp-camera
git clone https://github.com/espressif/esp32-camera components/esp32-camera
idf.py set-target esp32
idf.py menuconfig
idf.py flash monitor
```

# Start firmware
Change GPIO0 to open and press the RESET button.

# Configuration
Set the following items using menuconfig.

![config-main](https://user-images.githubusercontent.com/6020549/118468330-9c6ace00-b73f-11eb-9ed0-49e6084f3dcf.jpg)
![config-app](https://user-images.githubusercontent.com/6020549/118947420-48a3f300-b992-11eb-8b4f-51b2980e2af7.jpg)

## File system Selection

ESP32 supports the following file systems.   
- SPIFFS file system on FLASH   
- FAT file system on FLASH   
- FAT file system on SPI peripheral SDCARD   
- FAT file system on SDMMC peripheral SDCARD   

You can select any one using menuconfig.   

![config-filesystem](https://user-images.githubusercontent.com/6020549/98636628-9c41de00-236a-11eb-907f-c4c293a1b577.jpg)

### Using FAT file system on SPI peripheral SDCARD

__Must be formatted with FAT32 before use__

|ESP32 pin|SPI pin|Notes|
|:-:|:-:|:-:|
|GPIO14(MTMS)|SCK||
|GPIO15(MTDO)|MOSI|10k pull up if can't mount|
|GPIO2|MISO||
|GPIO13(MTCK)|CS|| 
|3.3V|VCC|Can't use 5V supply|
|GND|GND||

### Using FAT file system on SDMMC peripheral SDCARD

__Must be formatted with FAT32 before use__

|ESP32 pin|SD card pin|Notes|
|:-:|:-:|:-:|
|GPIO14(MTMS)|CLK|10k pullup|
|GPIO15(MTDO)|CMD|10k pullup|
|GPIO2|D0|10k pullup|
|GPIO4|D1|not used in 1-line SD mode; 10k pullup in 4-line SD mode|
|GPIO12(MTDI)|D2|not used in 1-line SD mode; 10k pullup in 4-line SD mode|
|GPIO13 (MTCK)|D3|not used in 1-line SD mode, but card's D3 pin must have a 10k pullup
|N/C|CD|optional, not used in the example|
|N/C|WP|optional, not used in the example|

## Wifi Setting

![config-wifi-1](https://user-images.githubusercontent.com/6020549/118470621-cde49900-b741-11eb-8038-fbb6f8053afc.jpg)

You can use the mDNS hostname instead of the IP address.   
![config-wifi-2](https://user-images.githubusercontent.com/6020549/118471770-12246900-b743-11eb-9619-4989f2fb3957.jpg)

You can use static IP.   
![config-wifi-3](https://user-images.githubusercontent.com/6020549/118470645-d4731080-b741-11eb-85c0-cad1a497e499.jpg)



## FTP server Setting

![config-ftpserver](https://user-images.githubusercontent.com/6020549/98636743-ca272280-236a-11eb-9094-7ec4d07ebcac.jpg)

## Remote File Name Setting

You can choose the file name on the FTP server side from the following.   
- Always the same file name   
- File name based on date and time   
When you choose date and time file name, you will need an NTP server.   
The file name will be YYYYMMDD-hhmmss.jpg.   

![config-remote-file-1](https://user-images.githubusercontent.com/6020549/118946651-8f451d80-b991-11eb-94c4-8976a2ee67ba.jpg)
![config-remote-file-2](https://user-images.githubusercontent.com/6020549/118946780-b3086380-b991-11eb-96a5-a0ca8a604745.jpg)

- Add FrameSize to Remote file Name   
When this is enabled, FrameSize is added to remote file name like this.   
`20210520-165740_800x600.jpg`   

![config-remote-file-3](https://user-images.githubusercontent.com/6020549/118946842-c0bde900-b991-11eb-83ef-ceb4a197a026.jpg)

## Select Frame Size
Large frame sizes take longer to take a picture.   
![config-framesize-1](https://user-images.githubusercontent.com/6020549/118947689-8bfe6180-b992-11eb-8657-b4e86d3acc70.jpg)
![config-framesize-2](https://user-images.githubusercontent.com/6020549/118947692-8d2f8e80-b992-11eb-9caa-1f6b6cb2210e.jpg)

## Select Shutter

You can choose one of the following shutter methods

- Shutter is the Enter key on the keyboard   
For operation check

![config-shutter-1](https://user-images.githubusercontent.com/6020549/99897436-d1d8b680-2cdc-11eb-8f6c-cea149cb845a.jpg)

- Shutter is a GPIO toggle

  - Initial Sate is PULLDOWN   
The shutter is prepared when it is turned from OFF to ON, and a picture is taken when it is turned from ON to OFF.   

  - Initial Sate is PULLUP   
The shutter is prepared when it is turned from ON to OFF, and a picture is taken when it is turned from OFF to ON.   

I confirmed that the following GPIO can be used.   

|GPIO|PullDown|PullUp|
|:-:|:-:|:-:|
|GPIO12|OK|NG|
|GPIO13|OK|OK|
|GPIO14|OK|OK|
|GPIO15|OK|OK|
|GPIO16|NG|NG|

![config-shutter-2](https://user-images.githubusercontent.com/6020549/99897437-d2714d00-2cdc-11eb-8e59-c8bf4ef25d62.jpg)

- Shutter is TCP Socket   
You can use tcp_send.py.   
`python ./tcp_send.py`

![config-shutter-3](https://user-images.githubusercontent.com/6020549/99897438-d2714d00-2cdc-11eb-8ab0-2838b1051a16.jpg)

- Shutter is UDP Socket   
You can use udp_send.py.   
Requires netifaces.   
`python ./udp_send.py`

![config-shutter-4](https://user-images.githubusercontent.com/6020549/99897493-3a279800-2cdd-11eb-91eb-87747cc28876.jpg)

- Shutter is HTTP Request   
You can use this command.   
`curl "http://esp32-camera.local:8080/take_picture"`

![config-shutter-5](https://user-images.githubusercontent.com/6020549/99897502-4ca1d180-2cdd-11eb-92a7-7f1075d10920.jpg)

## Flash Light

ESP32-CAM by AI-Thinker has a flash light on GPIO4.

![config-flash](https://user-images.githubusercontent.com/6020549/99897463-0cdaea00-2cdd-11eb-93ab-6c9540488639.jpg)

## Built-in WEB Server
You can check the photos taken using the built-in WEB server.

![browser](https://user-images.githubusercontent.com/6020549/124227364-837a7880-db45-11eb-9d8b-fa15c676adac.jpg)

# References   
https://github.com/nopnop2002/esp-idf-ftpServer

https://github.com/nopnop2002/esp-idf-ftpClient

