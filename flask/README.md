# FTP Client using flask
You can take & view pictures using WEB interface.   
This application works as an FTP client.   
This application will create a new file (shutter.txt) on the FTP server.   

![slide4](https://user-images.githubusercontent.com/6020549/187132648-23848e79-3f55-4a69-ac04-f761ebd5ed94.JPG)

# Install flask & pyftpdlib
```
sudo apt install python3-pip python3-setuptools
python3 -m pip install -U pip
python3 -m pip install -U wheel
python3 -m pip install -U Werkzeug
python3 -m pip install -U flask
python3 -m pip install -U pyftpdlib
```

# Start WEB Server using Flask
```
git clone https://github.com/nopnop2002/esp-idf-ftp-camera
cd esp-idf-ftp-camera/flask
python3 ftp-client.py --help
```

![Image](https://github.com/user-attachments/assets/6f3b3876-c125-4915-b025-2f35e379cb0d)

Specify the FTP server information at startup.   
![Image](https://github.com/user-attachments/assets/39ea5532-2319-446e-9d9f-d95c9e4b3c23)

Open your browser and enter the IP address of your flask server in the address bar.   
![Image](https://github.com/user-attachments/assets/dfbeb83e-942b-46f1-ba1e-de5eb00504dd)

# Start ESP32
ESP32 shutter trigger must be configured with Triggered on remote file creation.   
ESP32 periodically monitors the FTP server for file creation.   
![Image](https://github.com/user-attachments/assets/75bada2d-3107-45c9-8347-813bf1033a74)

# Take picture
Take Picutre will create a new file (shutter.txt) on the FTP server.   
This will cause the ESP32 to take a photo and transfer the image to an FTP server.   
![Image](https://github.com/user-attachments/assets/2cbd5ce0-92cc-4680-a485-856f82addf95)

Refresh web page.   
![Image](https://github.com/user-attachments/assets/d1736c9f-71fb-487f-80a4-1f7cc706604d)
![Image](https://github.com/user-attachments/assets/0d647322-3f6c-4130-88ed-6ef25815f01e)
