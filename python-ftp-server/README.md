# FTP Server using python

You can build FTP Server using python.   
Tutorial is [here](https://pyftpdlib.readthedocs.io/en/latest/tutorial.html).   

# Installation   
```
cd esp-idf-ftp-camera/python-ftp-server
python3 -m pip install pyftpdlib

python3 main.py --help
usage: main.py [-h] [--user USER] [--password PASSWORD] [--port PORT]

optional arguments:
  -h, --help           show this help message and exit
  --user USER          ftp user name
  --password PASSWORD  ftp user password
  --port PORT          ftp port
```

# Default parameters   
- user:ftpuser   
- password:ftppass   
- port:2121   

# Screen Shot
```
[I 2026-01-14 17:14:25] concurrency model: async
[I 2026-01-14 17:14:25] masquerade (NAT) address: None
[I 2026-01-14 17:14:25] passive ports: None
[I 2026-01-14 17:14:25] >>> starting FTP server on 0.0.0.0:2121, pid=10674 <<<
[I 2026-01-14 17:14:27] 192.168.0.160:58884-[] FTP session opened (connect)
[I 2026-01-14 17:14:27] 192.168.0.160:58884-[ftpuser] USER 'ftpuser' logged in.
[I 2026-01-14 17:14:27] 192.168.0.160:58884-[ftpuser] STOR /home/nop/rtos/esp-idf-ftpClient/python-ftp-server/20260114-171427_640x480.jpg completed=1 bytes=10354 seconds=0.033
on_file_received=/home/nop/rtos/esp-idf-ftpClient/python-ftp-server/20260114-171427_640x480.jpg
[I 2026-01-14 17:14:27] 192.168.0.160:58884-[ftpuser] FTP session closed (disconnect).
```
