# esp-idf-remote-camera
Remote camera for esp-idf.   
Take a picture and Publish it via FTP.   

![スライド1](https://user-images.githubusercontent.com/6020549/98742099-2f255b80-23f1-11eb-9495-282ae7e21569.JPG)

![スライド2](https://user-images.githubusercontent.com/6020549/98742111-32b8e280-23f1-11eb-8581-20df212ca143.JPG)

# Software requirements
esp-idf ver4.1 or later.   

# Install
Use a USB-TTL converter.   

|ESP-32|USB-TTL|
|:-:|:-:|
|U0TXD|RXD|
|U0RXD|TXD|
|GPIO0|GND|
|5V|5V|
|GND|GND|


```
git clone https://github.com/nopnop2002/esp-idf-remote-camera
cd esp-idf-remote-camera
git clone https://github.com/espressif/esp32-camera components
make menuconfig
make flash monitor
```

# Start firmware
Change GPIO0 to open and press the RESET button.

# Configuration
Set the following items using menuconfig.

![config-main](https://user-images.githubusercontent.com/6020549/98636527-63a20480-236a-11eb-8f9a-691084f226bc.jpg)

![config-menu](https://user-images.githubusercontent.com/6020549/98636606-90561c00-236a-11eb-9055-62a564d5b18b.jpg)

## File system Selection

ESP32 supports the following file systems.   
1.SPIFFS file system on FLASH   
2.FAT file system on FLASH   
3.FAT file system on SPI peripheral SDCARD   
4.FAT file system on SDMMC peripheral SDCARD   

You can select any one using menuconfig.   

![config-filesystem](https://user-images.githubusercontent.com/6020549/98636628-9c41de00-236a-11eb-907f-c4c293a1b577.jpg)

- Using FAT file system on SPI peripheral SDCARD

__Must be formatted with FAT32 before use__

|ESP32 pin|SPI pin|Notes|
|:-:|:-:|:-:|
|GPIO14(MTMS)|SCK||
|GPIO15(MTDO)|MOSI|10k pull up if can't mount|
|GPIO2|MISO||
|GPIO13(MTCK)|CS|| 
|3.3V|VCC|Can't use 5V supply|
|GND|GND||

- Using FAT file system on SDMMC peripheral SDCARD

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

![config-wifi](https://user-images.githubusercontent.com/6020549/98636701-bda2ca00-236a-11eb-9379-0084435b3e52.jpg)

## FTP server Setting

![config-ftpserver](https://user-images.githubusercontent.com/6020549/98636743-ca272280-236a-11eb-9094-7ec4d07ebcac.jpg)

## Remote File Name Setting

You can choose the file name on the FTP server side from the following.   
- Always the same file name   
- File name based on date and time   
When you choose date and time file name, you will need an NTP server.
The file name will be YYYYMMDD-hhmmss.jpg.   

![config-remote-file-1](https://user-images.githubusercontent.com/6020549/98636788-dc08c580-236a-11eb-9ebc-e38362b7a072.jpg)
![config-remote-file-2](https://user-images.githubusercontent.com/6020549/98636804-e1fea680-236a-11eb-8425-0831c905962b.jpg)

## Camera Pin

![config-camerapin](https://user-images.githubusercontent.com/6020549/98636895-06f31980-236b-11eb-8ba5-e92ccb79700d.jpg)

## Picture Size

![config-picturesize](https://user-images.githubusercontent.com/6020549/98636918-11adae80-236b-11eb-817c-3c6a9be82866.jpg)

## Select Shutter

You can choose one of the following shutter methods

- Shutter is the Enter key on the keyboard   
For operation check

![config-shutter-1](https://user-images.githubusercontent.com/6020549/98636952-1e320700-236b-11eb-8c5b-0a2b56267310.jpg)

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

![config-shutter-2](https://user-images.githubusercontent.com/6020549/98636945-1c684380-236b-11eb-8682-32e8430d7216.jpg)

- Shutter is TCP Socket   

You can connect with mDNS hostname.   
You can use tcp_send.py.   
```
python ./tcp_send.py
```

![config-shutter-3](https://user-images.githubusercontent.com/6020549/98636948-1d997080-236b-11eb-8579-b312da1ac915.jpg)

- Shutter is UDP Socket   
You can use udp_send.py.   
```
python ./udp_send.py
```

![config-shutter-4](https://user-images.githubusercontent.com/6020549/98636950-1d997080-236b-11eb-98ca-c14d16954f42.jpg)

## Flash Light

ESP32-CAM by AI-Thinker have flash light on GPIO4.

![config-flash](https://user-images.githubusercontent.com/6020549/98637034-46216a80-236b-11eb-8504-e83f718f5e85.jpg)

