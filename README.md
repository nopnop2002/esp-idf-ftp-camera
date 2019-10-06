# esp-idf-remote-camera
Remote camera for esp-idf

```
git clone https://github.com/nopnop2002/esp-idf-remote-camera
cd esp-idf-remote-camera
mkdir components
git clone https://github.com/espressif/esp32-camera components
make menuconfig
make flash monitor
```

Set the following items using menuconfig.

![config-main](https://user-images.githubusercontent.com/6020549/66263277-d0270780-e82a-11e9-8d2e-16e020a897c2.jpg)
![config](https://user-images.githubusercontent.com/6020549/66263086-38c0b500-e828-11e9-904c-42bf5804ff50.jpg)

# File system

ESP32 supports the following file systems.   
1.SPIFFS file system on FLASH   
2.FAT file system on FLASH   
3.FAT file system on SPI peripheral SDCARD   
4.FAT file system on SDMMC peripheral SDCARD   

You can select any one using menuconfig.   

![config-filesystem](https://user-images.githubusercontent.com/6020549/66263087-3bbba580-e828-11e9-8bd5-36137eb4627d.jpg)

# Wifi

![config-wifi](https://user-images.githubusercontent.com/6020549/66263088-42e2b380-e828-11e9-9ea0-2191f99db05e.jpg)

# FTP server

You can choose the file name on the FTP server side from the following.   
1.Always the same file name   
2.File name based on date and time   

![config-ftpserver](https://user-images.githubusercontent.com/6020549/66263096-483ffe00-e828-11e9-8452-25518d18bd8b.jpg)

When you choose date and time file name, you will need an NTP server.
![config-ftpserver-2](https://user-images.githubusercontent.com/6020549/66263303-78d56700-e82b-11e9-9290-067f6b9f974e.jpg)

# Camera Pin

![config-camerapin](https://user-images.githubusercontent.com/6020549/66263115-55f58380-e828-11e9-87ab-a57c6ec999df.jpg)

# Picutre Size

![config-picturesize](https://user-images.githubusercontent.com/6020549/66263117-60b01880-e828-11e9-9839-8c38edd0ced2.jpg)

# Shutter method

You can choose one of the following shutter methods

![config-shutter-2](https://user-images.githubusercontent.com/6020549/66263121-660d6300-e828-11e9-9c8f-8c12a8f3f2a7.jpg)

## Shutter is the Enter key on the keyboard   
For operation check

![config-shutter-1](https://user-images.githubusercontent.com/6020549/66263120-6574cc80-e828-11e9-980b-b495e9f59e8e.jpg)

## Shutter is a GPIO toggle

Initial Sate is PULLDOWN   
When it turns from OFF to ON, it prepares the shutter and takes a picture again when it is OFF.   

Initial Sate is PULLUP   
When it turns from ON to OFF, it prepares the shutter and takes a picture again when it is ON.   

![config-shutter-3](https://user-images.githubusercontent.com/6020549/66263122-660d6300-e828-11e9-8619-e54ab0c6b54f.jpg)

## Shutter is TCP Socket   

You can use tcp_send.py.   
```
python ./tcp_send.py
```
![config-shutter-4](https://user-images.githubusercontent.com/6020549/66263123-660d6300-e828-11e9-897e-137533cd246f.jpg)


## Shutter is UDP Socket
You can use udp_send.py.   
```
python ./udp_send.py
```

![config-shutter-5](https://user-images.githubusercontent.com/6020549/66263119-6574cc80-e828-11e9-96f0-9a48bc2fe014.jpg)



