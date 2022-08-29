# -*- coding: utf-8 -*-
import os
import sys
import datetime
import argparse
import logging
from flask import Flask, render_template, request, jsonify, Blueprint, redirect, url_for, send_file
from werkzeug.serving import WSGIRequestHandler
from ftplib import FTP

logging.basicConfig(format='%(levelname)s:%(message)s', level=logging.INFO)
parser = argparse.ArgumentParser()
parser.add_argument('--server', default="localhost", help='ftp server')
parser.add_argument('--user', default="user", help='ftp user')
parser.add_argument('--password', default="password", help='ftp password')
parser.add_argument('--directory', default="", help='ftp start directory. default = $HOME')
parser.add_argument('--shutter', default="shutter.txt", help='shutter triger file. default = shutter.txt')
args = parser.parse_args()
logging.info("server={} {}".format(type(args.server), args.server))
logging.info("user={} {}".format(type(args.user), args.user))
logging.info("password={} {}".format(type(args.password), args.password))
logging.info("directory={} {}".format(type(args.directory), args.directory))
logging.info("shutter={} {}".format(type(args.shutter), args.shutter))

app = Flask(__name__)

# create DOWNLOAD_DIR
DOWNLOAD_DIR = os.path.join(os.getcwd(), "downloaded")
logging.info("DOWNLOAD_DIR={}".format(DOWNLOAD_DIR))
if (os.path.exists(DOWNLOAD_DIR) == False):
	logging.warning("DOWNLOAD_DIR [{}] not found. Create this".format(DOWNLOAD_DIR))
	os.mkdir(DOWNLOAD_DIR)

# Added /downloaded to static_url_path
add_app = Blueprint("downloaded", __name__, static_url_path='/downloaded', static_folder='./downloaded')
app.register_blueprint(add_app)

def download_from_server(dirname, filename):
	function = sys._getframe().f_code.co_name
	logging.info("{}:dirname={} filename={}".format(function, dirname, filename))
	ftp = FTP(args.server)
	ret = ftp.login(args.user, args.password)
	if (len(dirname) > 0):
		ret = ftp.cwd(dirname)
		logging.info("{}:ftp.cwd={}".format(function, ret))
	ftpcmd = "RETR {}".format(os.path.basename(filename))
	logging.info("{}:ftpcmd={}".format(function, ftpcmd))
	ext = os.path.splitext(filename)
	if (ext[1] == ".jpg" or ext[1] == ".jpeg"):
		with open(filename, "wb") as f:
			ret = ftp.retrbinary(ftpcmd, f.write)
			logging.info("{}:ftp.retrbinary={}".format(function, ret))
	else:
		with open(filename, "w") as f:
			ret = ftp.retrlines(ftpcmd, f.write)
			logging.info("{}:ftp.retrlines={}".format(function, ret))

	ftp.quit()

def delete_from_server(dirname, filename):
	function = sys._getframe().f_code.co_name
	logging.info("{}:dirname={} filename={}".format(function, dirname, filename))
	ftp = FTP(args.server)
	ret = ftp.login(args.user, args.password)
	if (len(dirname) > 0):
		ret = ftp.cwd(dirname)
		logging.info("{}:ftp.cwd={}".format(function, ret))
	ret = ftp.delete(os.path.basename(filename))
	logging.info("{}:ftp.delete={}".format(function, ret))
	ftp.quit()

def upload_to_server(filename):
	function = sys._getframe().f_code.co_name
	print("args.shutter={}".format(args.shutter))
	path = os.path.dirname(args.shutter)
	print("path=[{}] {}".format(path, len(path)))
	ftp = FTP(args.server)
	ret = ftp.login(args.user, args.password)
	if (len(path)):
		try:
			ret = ftp.mkd(path)
			print("ftp.mkd={}".format(ret))
		except Exception:
			pass
		ret = ftp.cwd(path)
		print("ftp.cwd={}".format(ret))
	#ftpcmd = "STOR {}".format(os.path.basename(filename))
	ftpcmd = "STOR {}".format(os.path.basename(args.shutter))
	logging.info("{}:ftpcmd={}".format(function, ftpcmd))
	with open(filename, "rb") as f:
		ftp.storlines(ftpcmd, f)
	logging.info("{}:ftp.delete={}".format(function, ret))
	ftp.quit()



@app.route("/")
def root():
	function = sys._getframe().f_code.co_name

	files = []
	dirs = []
	meta = {
		"ftp_server": args.server,
		"ftp_user": args.user,
		"ftp_password": args.password,
		"ftp_directory": app.config['directory']
	}
	logging.info("{}:meta={}".format(function, meta))

	if (len(app.config['directory']) != 0):
		dirs.append({
			"name": app.config['directory'],
			"visible": False
		})
		dirs.append({
			"name": "..",
			"visible": True
		})
	
	

	ftp = FTP(args.server)
	ret = ftp.login(args.user, args.password)
	#print("ftp.login={}".format(ret))
	ret = ftp.cwd(app.config['directory'])
	#print("ftp.cwd={}".format(ret))
	print(ftp.retrlines('LIST'))
	#files = ftp.nlst(".")
	#print("ftp.nlst={}".format(files))

	for name in ftp.nlst("."):
		print("name={}".format(name))
		# check is directory
		try:
			isDirectory = False
			file_size = ftp.size(name)
		except Exception:
			isDirectory = True
			file_size = 0
		print("isDirectory={}".format(isDirectory))
		print("file.size={} {}".format(type(file_size), file_size))

		if (isDirectory):
			dirs.append({
				"name": name,
				"visible": True
			})

		else:
			ext = os.path.splitext(name)
			print("ext={}".format(ext))
			visible = False
			if (ext[1] == ".jpg" or ext[1] == ".jpeg"):
				visible = True
			file_datetime = ftp.voidcmd("MDTM " + name).split(' ')
			print("file_datetime={}".format(file_datetime))
			file_date = file_datetime[1][0:8]
			file_date = file_date[0:4] + "/" + file_date[4:6] + "/" + file_date[6:8]
			file_time = file_datetime[1][8:]
			file_time = file_time[0:2] + ":" + file_time[2:4]
			print("file_date={}".format(file_date))
			print("file_time={}".format(file_time))
			fullname = "{}/{}".format(DOWNLOAD_DIR, name)
			print("fullname={}".format(fullname))

			files.append({
				"name": name,
				"size": str(file_size) + " B",
				"date": file_date,
				"time": file_time,
				"fullname": fullname,
				"visible": visible
			})
	ftp.quit()
	logging.info(files)


	return render_template("index.html", files=sorted(files, key=lambda k: k["name"].lower()), folders=dirs, meta=meta)

@app.route("/chdir")
def chdir():
	function = sys._getframe().f_code.co_name
	filename = request.args.get('filename', default=None, type=str)
	logging.info("{}:filename={}".format(function, filename))
	print("directory={} [{}]".format(len(app.config['directory']), app.config['directory']))
	if (filename == ".."):
		dirs = app.config['directory'].split("/")
		print("dirs={} {}".format(dirs, len(dirs)))
		app.config['directory'] = ""
		for i in range(len(dirs)-2):
			app.config['directory'] = app.config['directory'] + dirs[i] + "/"
			print("directory={} [{}]".format(len(app.config['directory']), app.config['directory']))

	else:
		if (len(app.config['directory']) == 0):
			app.config['directory'] = filename + "/"
		else:
			app.config['directory'] = app.config['directory'] + filename + "/"

	return redirect(url_for('root'))

@app.route("/download")
def download():
	function = sys._getframe().f_code.co_name
	filename = request.args.get('filename', default=None, type=str)
	logging.info("{}:filename={}".format(function, filename))

	if os.path.isfile(filename):
		if os.path.dirname(filename) == DOWNLOAD_DIR.rstrip("/"):
			return send_file(filename, as_attachment=True)
		else:
			return render_template("no_permission.html")
	else:
		download_from_server(app.config['directory'], filename)
		return send_file(filename, as_attachment=True)
	return None

@app.route("/imageview")
def imageview():
	function = sys._getframe().f_code.co_name
	filename = request.args.get('filename', default=None, type=str)
	logging.info("{}:filename={}".format(function, filename))
	rotate = request.args.get('rotate', default=0, type=int)
	logging.info("{}:rotate={}{}".format(function, rotate, type(rotate)))

	if (os.path.isfile(filename) == False):
		download_from_server(app.config['directory'], filename)

	logging.info("{}:filename={}".format(function, filename))
	filename = filename.replace(os.getcwd(), "")
	logging.info("{}:filename={}".format(function, filename))
	return render_template("view.html", user_image = filename, rotate=rotate)

@app.route("/delete")
def delete():
	function = sys._getframe().f_code.co_name
	filename = request.args.get('filename', default=None, type=str)
	logging.info("{}:filename={}".format(function, filename))
	if (os.path.isfile(filename)):
		os.remove(filename)
	delete_from_server(app.config['directory'], filename)
	return redirect(url_for('root'))

@app.route("/publish")
def publish():
	function = sys._getframe().f_code.co_name
	logging.info("{}".format(function))
	filename = "{}/shutter.txt".format(DOWNLOAD_DIR)
	print("filename={}".format(filename))
	f = open(filename, 'w')
	f.write('')
	f.close()
	upload_to_server(filename)
	return redirect(url_for('root'))

if __name__ == '__main__':
	WSGIRequestHandler.protocol_version = "HTTP/1.1"
	#app.run(host='127.0.0.1', port=5000)
	app.config['directory'] = args.directory
	if (len(app.config['directory']) > 0):
		lastchar = app.config['directory'][-1:]
		print("lastchar={}".format(lastchar))
		if (lastchar != "/"):
			app.config['directory'] = app.config['directory'] + "/"
	print("directory={}".format(app.config['directory']))
	app.run(host='0.0.0.0', port=8080, debug=True)
