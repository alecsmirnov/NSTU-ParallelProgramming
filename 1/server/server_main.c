#include <stdio.h>
#include <string.h>
#include <math.h>

#include "server.h"

#define ARGS_COUNT 4

#define TEXT_QUOTE(...) #__VA_ARGS__

#define intDigitsCount(val)	\
	((val) == 0 ? 1 : (size_t)floor(log10(abs(val))) + 1)

static const char* RESPONSE_TEMPLATE = TEXT_QUOTE(
	HTTP/1.0 200 OK\r\n\r\n
		<html>
			<head>
				<title>Request</title>
				<style>
					h1 {
						text-align: center;
					}
				</style>
			</head>
		<body>
			<h1>Request number %u has been processed</h1>
		</body>
	</html>\r\n
);

static const char* RESPONSE_TEMPLATE_PHP = TEXT_QUOTE(
	HTTP/1.1 200 OK\r\n\r\n
		<html>
			<head>
				<title>Request</title>
				<style>
					h1, div {
						text-align: center;
					}
				</style>
			</head>
		<body>
			<h1>Request number %u has been processed</h1>
			<div>PHP version: %s</div>
		</body>
	</html>\r\n
);

static void* threadFunc(void* arg) {
	ThreadParam* thread_param = (ThreadParam*)arg;

	size_t response_size = strlen(RESPONSE_TEMPLATE) + intDigitsCount(thread_param->request_num);
	char* response = (char*)malloc(sizeof(char) * response_size);

	snprintf(response, response_size, RESPONSE_TEMPLATE, thread_param->request_num);

	clientWrite(thread_param->client_fd, response, response_size);

	clientClose(thread_param->client_fd);
	free(response);
	pthread_exit(NULL);
}

static void* threadFuncPHP(void* arg) {
	ThreadParam* thread_param = (ThreadParam*)arg;

	enum {PHP_VERSION_LEN = 20};
	size_t response_size = strlen(RESPONSE_TEMPLATE_PHP) + intDigitsCount(thread_param->request_num) + PHP_VERSION_LEN;
	char* response = (char*)malloc(sizeof(char) * response_size);

	FILE* fp = popen("php -r \"echo phpversion();\"", "r");
	char php_version[PHP_VERSION_LEN];
	fscanf(fp, "%s", php_version);
	pclose(fp);

	snprintf(response, response_size, RESPONSE_TEMPLATE_PHP, thread_param->request_num, php_version);
	
	clientWrite(thread_param->client_fd, response, response_size);

	clientClose(thread_param->client_fd);
	free(response);
	pthread_exit(NULL);
}

int main(int argc, char* argv[]) {
	if (argc < ARGS_COUNT) {
		fprintf(stderr, "Wrong number of arguments!\n");
		fprintf(stderr, "Enter: <func num> <stack size num> <clear pull>\n");
		fprintf(stderr, "(Func number: 0 - std, 1 - php; Stack size num: 0 - 512 KB, 1 - 1 MB, 2 - 2 MB)\n");
		exit(EXIT_FAILURE);
	}

	uint8_t func_num = atoi(argv[1]);
	uint8_t stack_size_num = atoi(argv[2]);
	size_t clear_pull = atol(argv[3]);

	pthread_func thread_func = threadFunc;
	switch (func_num) {
		case 0: thread_func = threadFunc;    break;
		case 1: thread_func = threadFuncPHP; break;
	}

	size_t stack_size = 2 * MB;
	switch (stack_size_num) {
		case 0: stack_size = 512 * KB; break;
		case 1: stack_size =   1 * MB; break;
		case 2: stack_size =   2 * MB; break;
	}

	serverStart(thread_func, stack_size, clear_pull);
	
	return 0;
}