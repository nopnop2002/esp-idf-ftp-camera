# esp-idf-ftp-camera
Take a picture and Publish it via FTP.   
This project use [ESP32 Camera Driver](https://github.com/espressif/esp32-camera).   
This project use [ESP32-FTP-Client](https://github.com/JohnnyB1290/ESP32-FTP-Client).   

![slide1](https://user-images.githubusercontent.com/6020549/187340044-1b84534c-d2ac-459b-81d9-77de0c593a42.JPG)
![slide2](https://user-images.githubusercontent.com/6020549/119622017-b98b5500-be41-11eb-8ad6-1bde963e7849.JPG)
![slide3](https://user-images.githubusercontent.com/6020549/187340133-739c03a4-8b86-4b75-9d75-79a565fbc3cf.JPG)

# Hardware requirements
ESP32 development board with OV2640 camera.   
If you use other camera, edit sdkconfig.default.   
![ESP32-Camera-board](https://user-images.githubusercontent.com/6020549/198520670-27ea9bd1-71d8-438f-be58-8516154be4af.JPG)


# Software requirements
esp-idf v4.4/v5.0.   

# Installation
For AiThinker ESP32-CAM, You have to use a USB-TTL converter.   

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
For AiThinker ESP32-CAM, Change GPIO0 to open and press the RESET button.

# Configuration
Set the following items using menuconfig.

![config-main](https://user-images.githubusercontent.com/6020549/118468330-9c6ace00-b73f-11eb-9ed0-49e6084f3dcf.jpg)
![config-app](https://user-images.githubusercontent.com/6020549/198521329-cd8e8a45-92df-4b3e-9445-fe762b2446bb.jpg)

## Wifi Setting

![config-wifi-1](https://user-images.githubusercontent.com/6020549/160319603-bd916f04-d149-499c-88a9-0182d7479c10.jpg)

You can connect using the mDNS hostname instead of the IP address.   
![config-wifi-2](https://user-images.githubusercontent.com/6020549/160319623-de65c66b-dfe1-4886-9ba6-986900b62c91.jpg)

You can use static IP.   
![config-wifi-3](https://user-images.githubusercontent.com/6020549/160319234-4c7ee824-e833-4cf9-bed3-2b25ffe29066.jpg)

## Using mDNS hostname
You can connect using the mDNS hostname instead of the IP address.   
- esp-idf V4.3 or earlier   
 You will need to manually change the mDNS strict mode according to [this](https://github.com/espressif/esp-idf/issues/6190) instruction.   
- esp-idf V4.4  
 If you set CONFIG_MDNS_STRICT_MODE = y in sdkconfig.defaults, the firmware will be built with MDNS_STRICT_MODE.   
 __If MDNS_STRICT_MODE is not set, mDNS name resolution will not be possible after long-term operation.__   
- esp-idf V4.4.1   
 mDNS component has been updated.   
 If you set CONFIG_MDNS_STRICT_MODE = y in sdkconfig.defaults, the firmware will be built with MDNS_STRICT_MODE.   
 __Even if MDNS_STRICT_MODE is set, mDNS name resolution will not be possible after long-term operation.__   
- esp-idf V5.0 or later   
 mDNS component has been updated.   
 Long-term operation is possible without setting MDNS_STRICT_MODE.   
 The following lines in sdkconfig.defaults should be removed before menuconfig.   
 ```CONFIG_MDNS_STRICT_MODE=y```

## FTP server Setting
You can specify mDNS hostname for the FTP server.   
![config-ftpserver-1](https://user-images.githubusercontent.com/6020549/187012952-f7ef4541-64dd-4a1a-8dbd-b8c19b0a69b2.jpg)

Store pictures in FTP Server subdirectories.   
![config-ftpserver-2](https://user-images.githubusercontent.com/6020549/187012954-f6e79b78-813f-4072-854e-6d1f592f895a.jpg)

Create index.html on FTP Server.   
When your FTP server acts as an HTTP server, You can view picture using browser.   
![config-ftpserver-3](https://user-images.githubusercontent.com/6020549/192081871-b73ea69c-ca51-494c-a064-181707bf7b25.jpg)

You can specify the rotation of the image.   
![config-ftpserver-4](https://user-images.githubusercontent.com/6020549/192081894-db0d5b69-57f2-4e68-916f-13c3ddef77e4.jpg)

My FTP server acts as an HTTP server.   
![rotate-000](https://user-images.githubusercontent.com/6020549/187013052-0991b3e7-d781-4661-925a-fe96fbb403af.jpg)
![rotete-090](https://user-images.githubusercontent.com/6020549/187013054-f6c8d8fa-835a-448e-b9a3-ec03fa2fec94.jpg)




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


## Select Board
![config-board](https://user-images.githubusercontent.com/6020549/198521553-cac7bb32-ad8e-4d9a-9f2f-36aa5a3f5ec1.jpg)


## Select Frame Size
Large frame sizes take longer to take a picture.   
![config-framesize-1](https://user-images.githubusercontent.com/6020549/118947689-8bfe6180-b992-11eb-8657-b4e86d3acc70.jpg)
![config-framesize-2](https://user-images.githubusercontent.com/6020549/118947692-8d2f8e80-b992-11eb-9caa-1f6b6cb2210e.jpg)

## Select Shutter

You can choose one of the following as shutter

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
You can use tcp_send.py as shutter.   
`python3 ./tcp_send.py`

![config-shutter-3](https://user-images.githubusercontent.com/6020549/99897438-d2714d00-2cdc-11eb-8ab0-2838b1051a16.jpg)

- Shutter is UDP Socket   
 You can use this command as shutter.   
 `echo "take" | socat - UDP-DATAGRAM:255.255.255.255:9876,broadcast`

 You can use udp_send.py as shutter.   
 Requires netifaces.   
 `python3 ./udp_send.py`

![config-shutter-4](https://user-images.githubusercontent.com/6020549/99897493-3a279800-2cdd-11eb-91eb-87747cc28876.jpg)

- Shutter is HTTP Request   
You can use this command as shutter.   
`curl "http://esp32-camera.local:8080/take/picture"`

![config-shutter-5](https://user-images.githubusercontent.com/6020549/192754978-cf5f505c-5f7b-4856-b6a3-692b2f94a80b.jpg)

- Shutter is Remote file polling   
Take a picture if there is a file on the FTP server.   
Check if the file exists at 10 second intervals.   

![config-shutter-61](https://user-images.githubusercontent.com/6020549/187130276-8b10092d-3f1f-41dc-b8ca-b8ce4f0c74e5.jpg)

You can choose to create a file or delete a file.   
If you use Android, deleting files is easier.   
![config-shutter-62](https://user-images.githubusercontent.com/6020549/187130281-3bd52d09-3153-46a7-a929-9f0209ea726e.jpg)

## Flash Light

ESP32-CAM by AI-Thinker has a flash light on GPIO4.

![config-flash](https://user-images.githubusercontent.com/6020549/187013402-5aac5709-ae8c-421f-978b-a0e321ff1cdb.jpg)


# View picture using Built-in WEB Server
You can view the pictures taken using the built-in WEB server.   
Enter the ESP32's IP address and port number in the address bar of your browser.   
You can use mDNS instead of IP address.   

![browser](https://user-images.githubusercontent.com/6020549/124227364-837a7880-db45-11eb-9d8b-fa15c676adac.jpg)

# Take & view pictures using flask
read [this](https://github.com/nopnop2002/esp-idf-ftp-camera/tree/master/flask).   

# Take pictures using Android
I use API Tester: HTTP Client app.   
You can download from [Google Play](https://play.google.com/store/apps/details?id=apitester.org).   
This app can easily issue HTTP requests.   
You can easily take pictures by setting HTTP Request to shutter.   
However, on Android, name resolution by mDNS is not possible.   

![api-tester](https://user-images.githubusercontent.com/6020549/192765579-d4883faa-b9e8-45d2-8c3e-53bc1f97b769.jpg)

# Take & view pictures using Android
I use X-plore File Manager app.   
You can download from [Google Play](https://www.lonelycatgames.com/apps/xplore).   
This app can easily view the files on the FTP server.   
This app can easily delete files on FTP server.   
It's much easier to delete a file than to create a new one.   
You can easily take pictures by setting remote file deletion to shutter.   
You can operate the ESP32 with just your fingertips.   
If you use a rental server, you can take pictures of London from New York.   
__Very Good!!__

![X-plore-1](https://user-images.githubusercontent.com/6020549/187131037-e66cadc5-cf82-41e0-96d8-5e996f4caacf.JPG)
![X-plore-2](https://user-images.githubusercontent.com/6020549/187131042-27051c15-9876-45b8-b884-e7cdecd02298.JPG)

![slide5](https://user-images.githubusercontent.com/6020549/187155277-900c5c14-556e-49c4-baf0-e1be57aba05f.JPG)

# References   
https://github.com/nopnop2002/esp-idf-ftpServer

https://github.com/nopnop2002/esp-idf-ftpClient

