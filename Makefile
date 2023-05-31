.PHONY : echo-client echo-server

all: echo-client echo-server

echo-client:
	cd client; make; cd ..

echo-server:
	cd server; make; cd ..

clean:
	cd client; make clean; cd ..
	cd server; make clean; cd ..

