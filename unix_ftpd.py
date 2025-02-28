#!/usr/bin/env python3

# Copyright (C) 2007 Giampaolo Rodola' <g.rodola@gmail.com>.
# Use of this source code is governed by MIT license that can be
# found in the LICENSE file.

"""A FTPd using local UNIX account database to authenticate users.

It temporarily impersonate the system users every time they are going
to perform a filesystem operations.
"""

import os
import queue
import threading
import time
import cv2
from pyftpdlib.authorizers import UnixAuthorizer
from pyftpdlib.filesystems import UnixFilesystem
from pyftpdlib.handlers import FTPHandler
from pyftpdlib.servers import FTPServer

queue01 = queue.Queue()
queue02 = queue.Queue()
imageFileName = None

def threadView(q1, q2):
	time.sleep(1)
	localVal = threading.local()
	while True:
		if q1.empty():
			time.sleep(1.0)
			#print("thread waiting..")
		else:
			localVal = q1.get()
			print("thread q1.get() localVal={}".format(localVal))
			print("imageFileName={}".format(imageFileName))
			image = cv2.imread(imageFileName)
			cv2.imshow('image', image)
			cv2.waitKey(0)
			cv2.destroyWindow('image')
			print("thread q2.put() localVal={}".format(localVal))
			q2.put(localVal)

class Handler(FTPHandler):
	def on_file_sent(self, file):
		print("send file!",file)
		#super(Handler, self).ftp_RETR(file)
	def on_file_received(self, file):
		print("received file!",file)
		#super(Handler, self).ftp_RETR(file)

		root, ext = os.path.splitext(file)
		print("root={} ext={}".format(root, ext))
		if (ext != ".jpg" and ext != ".jpeg"): return
		global imageFileName
		imageFileName = file
		queue01.put(0)
		while True:
			time.sleep(1)
			if queue02.empty():
				print("thread end waiting. ESC to end.")
				pass
			else:
				queue02.get()
				break
		print("thread end")

def main():
	authorizer = UnixAuthorizer(
		rejected_users=["root"], require_valid_shell=True
	)
	#handler = FTPHandler
	handler = Handler
	handler.authorizer = authorizer
	handler.abstracted_fs = UnixFilesystem
	server = FTPServer(('', 21), handler)
	server.serve_forever()

if __name__ == "__main__":
	thread = threading.Thread(target=threadView, args=(queue01,queue02,) ,daemon = True)
	thread.start()

	main()
