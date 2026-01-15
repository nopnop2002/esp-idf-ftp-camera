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
python3 -m pip install -U flask-mqtt
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
![Image](https://github.com/user-attachments/assets/efb50693-7019-4494-a3b4-adae9df1a942)

# Start ESP32
ESP32 shutter trigger must be configured with Triggered on remote file creation.   
ESP32 periodically monitors the FTP server for file creation.   
![Image](https://github.com/user-attachments/assets/75bada2d-3107-45c9-8347-813bf1033a74)

# Take picture
Take Picutre will create a new file (shutter.txt) on the FTP server.   
This will cause the ESP32 to take a photo and transfer the image to an FTP server.   
![flask-11](https://user-images.githubusercontent.com/6020549/187102690-e611e921-65da-4cea-bff7-16cc2b0d15cd.jpg)

Refresh web page.   
![flask-12](https://user-images.githubusercontent.com/6020549/187102694-920040c2-a333-43ed-97ad-1978c00ddbac.jpg)
![flask-13](https://user-images.githubusercontent.com/6020549/187102696-d95ce0ab-211d-4196-85c1-dc92c7d6a101.jpg)
