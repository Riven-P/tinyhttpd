all: tinyhttp

httpd: tinyhttp.c
	gcc -W -Wall -o tinyhttp tinyhttp.c -lpthread

clean:
	rm tinyhttp
