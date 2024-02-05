# FTP Client using flask
You can take & view pictures using WEB interface.   
This application works as an FTP client.   

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

![flask-1](https://user-images.githubusercontent.com/6020549/187102653-f30c4655-37ca-4db8-a926-e77a380cdc94.jpg)

You can specify an FTP server.   
![flask-2](https://github.com/nopnop2002/esp-idf-ftp-camera/assets/6020549/19394ad3-927f-47ee-b277-660aa48323c8)

# Take picture
ESP32 shutter trigger must be configured with Triggered on remote file creation.    
Take Picutre creates shutter.txt on the FTP server.   
![flask-11](https://user-images.githubusercontent.com/6020549/187102690-e611e921-65da-4cea-bff7-16cc2b0d15cd.jpg)

Refresh web page.   
![flask-12](https://user-images.githubusercontent.com/6020549/187102694-920040c2-a333-43ed-97ad-1978c00ddbac.jpg)
![flask-13](https://user-images.githubusercontent.com/6020549/187102696-d95ce0ab-211d-4196-85c1-dc92c7d6a101.jpg)
