include comport_sel.mk

.PHONY: build

build:  main/web/page/dist/index.html
	@idf.py build

f: build
	idf.py -p $(COM_PORT) flash

fm: f
	idf.py -p $(COM_PORT) monitor

m:
	idf.py -p $(COM_PORT) monitor

main/web/page/dist/index.html:
	@make -C main/web/page --no-print-directory

ota: build
	curl --request POST --data-binary "@build/booster.bin" http://192.168.4.1/update

preset:
	idf.py set-target esp32

clean:
	idf.py fullclean