#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef __linux__
#include <arpa/inet.h>
#include <sys/socket.h>
#endif // __linux
#ifdef WIN32
#include <winsock2.h>
#include "../mingw_net.h"
#endif // WIN32
#include <thread>
#include <vector>
#include <mutex>
#include <algorithm>

#ifdef WIN32
void perror(const char* msg) { fprintf(stderr, "%s %ld\n", msg, GetLastError()); }
#endif // WIN32

std::vector<int> client_vec;
std::mutex client_vec_lock;

void usage() {
	printf("syntax : echo-server <port> [-e[-b]]\n");
	printf("sample : echo-server 1234 -e -b\n");
}

struct Param {
	bool echo{false};
    	bool broadcast{false};
	uint16_t port{0};

	bool parse(int argc, char* argv[]) {
		port = atoi(argv[1]);
		for (int i = 2; i < argc; i++) {
			if (strcmp(argv[i], "-e") == 0) {
				echo = true;
				continue;
			} else if (strcmp(argv[i], "-b") == 0) {
				broadcast = true;
				continue;
			}
		}
		return port != 0;
	}
} param;

void recvThread(int sd) {
	printf("connected\n");
	static const int BUFSIZE = 65536;
	char buf[BUFSIZE];
	while (true) {
		ssize_t res = ::recv(sd, buf, BUFSIZE - 1, 0);
		if (res == 0 || res == -1) {
			fprintf(stderr, "recv return %ld", res);
			perror(" ");
			break;
		}
		buf[res] = '\0';
		printf("%s", buf);
		fflush(stdout);
		if (param.echo && !param.broadcast) {
			res = ::send(sd, buf, res, 0);
			if (res == 0 || res == -1) {
				fprintf(stderr, "send return %ld", res);
				perror(" ");
				break;
			}
		} 
		if (param.broadcast) {
			client_vec_lock.lock();
			for (int i: client_vec) {
				res = send(i, buf, res, 0);
				if (res == 0 || res == -1)
				{
				    fprintf(stderr, "send return %ld", res);
				    perror(" ");
				    break;
				}
			}
			client_vec_lock.unlock();
		}
	}

	client_vec_lock.lock();
	client_vec.erase(std::find(client_vec.begin(), client_vec.end(), sd));
	client_vec_lock.unlock();

	printf("disconnected\n");
	::close(sd);
}

int main(int argc, char* argv[]) {
	if (!param.parse(argc, argv)) {
		usage();
		return -1;
	}

#ifdef WIN32
	WSAData wsaData;
	WSAStartup(0x0202, &wsaData);
#endif // WIN32

	int sd = ::socket(AF_INET, SOCK_STREAM, 0);
	if (sd == -1) {
		perror("socket");
		return -1;
	}

	int res;
#ifdef __linux__
	int optval = 1;
	res = ::setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
	if (res == -1) {
		perror("setsockopt");
		return -1;
	}
#endif // __linux

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(param.port);

	ssize_t res2 = ::bind(sd, (struct sockaddr *)&addr, sizeof(addr));
	if (res2 == -1) {
		perror("bind");
		return -1;
	}

	res = listen(sd, 5);
	if (res == -1) {
		perror("listen");
		return -1;
	}

	while (true) {
		struct sockaddr_in cli_addr;
		socklen_t len = sizeof(cli_addr);
		int cli_sd = ::accept(sd, (struct sockaddr *)&cli_addr, &len);
		if (cli_sd == -1) {
			perror("accept");
			break;
		}

		std::thread* t = new std::thread(recvThread, cli_sd);
		t->detach();

		client_vec_lock.lock();
		client_vec.push_back(cli_sd);
		client_vec_lock.unlock();
	}
	::close(sd);
}
