set(COMPONENT_SRCS "main.c" 
	"FtpClient.c" 
	"ftp.c" 
	"gpio.c" 
	"keyboard.c" 
	"tcp_server.c" 
	"udp_server.c" 
	"http_server.c")
set(COMPONENT_ADD_INCLUDEDIRS ".")

register_component()
