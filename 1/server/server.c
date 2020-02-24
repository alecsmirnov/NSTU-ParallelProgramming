#include "server.h"

#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <signal.h>

#include "list.h"

// Код прохождения обработки
#define PTHREAD_PASS 0
// Код ошибки сокета
#define SOCK_ERROR  -1

// Функция обработки ошибок потоков
#define pthreadErrorHandle(test_code, pass_code, err_msg) do {		\
	if (test_code != pass_code) {									\
		fprintf(stderr, "%s: %s\n", err_msg, strerror(test_code));	\
		exit(EXIT_FAILURE);											\
	}																\
} while (0)

// Функция обработки ошибок сокетов
#define sockErrorHandle(test_code, err_code, serv_sock, err_msg) do {	\
	if (test_code == err_code) {										\
		fprintf(stderr, "%s: %s\n", err_msg, strerror(test_code));		\
		close(serv_sock);												\
		exit(EXIT_FAILURE);												\
	}																	\
} while (0)

// Проверка условия очистки данных: 0 - не чистить, n - чистить через каждые n записей
#define clearCheck(rec_num, clear_pull) \
	(clear_pull == 0 ? 0 : rec_num % clear_pull == 0)

// Все параметры потока
typedef struct ThreadInfo {
	// Открытые параметры
	uint32_t request_num;	// Номер запроса
	int client_fd;			// Дескриптор клиента

	// Закрытые параметры
	pthread_t thread_id;	// Идентификатор потока
} ThreadInfo;

// Отправить ответ клиенту
void clientWrite(int client_fd, char* response, size_t response_size) {
	write(client_fd, response, response_size);
}

// Закрытие соединения с клиентом
void clientClose(int client_fd) {
	shutdown(client_fd, SHUT_WR);
	recv(client_fd, NULL, 1, MSG_PEEK | MSG_DONTWAIT);
	close(client_fd);
}

// Запуск сервера с указанной функцией, размером стека и параметром очистки
// (Параметр очистки: 0 - не чистить, n - чистить через каждые n записей)
void serverStart(pthread_func thread_func, size_t stack_size, size_t clear_pull) {
	struct sockaddr_in serv_addr;
 
	int serv_sock = socket(AF_INET, SOCK_STREAM, 0);
	sockErrorHandle(serv_sock, SOCK_ERROR, serv_sock, "Socket open error"); 

 	char opt = 1;
	setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(SERVER_PORT);
 
	int err = bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
 	sockErrorHandle(err, SOCK_ERROR, serv_sock, "Bind error");
	
	err = listen(serv_sock, DEFAULT_CLIENT_COUNT); 
 	sockErrorHandle(err, SOCK_ERROR, serv_sock, "Listen error");

 	struct sockaddr_in client_addr;
	socklen_t sock_len = sizeof(client_addr);

	pthread_attr_t thread_attr;
	err = pthread_attr_init(&thread_attr);
	pthreadErrorHandle(err, PTHREAD_PASS, "Cannot create thread attribute");

	err = pthread_attr_setstacksize(&thread_attr, stack_size);
	pthreadErrorHandle(err, PTHREAD_PASS, "Setting thread stack size failed");

	// Инициализация структуры для хранения данных о поступающих соединениях
	List* threads_list = NULL;
	listInit(&threads_list, sizeof(ThreadInfo), NULL);

	pid_t serv_pid = getpid();

	// Приём входящих соединений в отдельных потоках
	uint32_t connection_count = 0;
	while (connection_count != UINT_MAX) {
		int client_fd = accept(serv_sock, (struct sockaddr*)&client_addr, &sock_len);
		if (SOCK_ERROR < client_fd) {
			printf("Server [%d]: got connection - %u\n", serv_pid, ++connection_count);

			ThreadInfo thread_param = (ThreadInfo){connection_count, client_fd};
			listPushBack(threads_list, (void*)&thread_param);
			
			err = pthread_create(&((ThreadInfo*)listBack(threads_list))->thread_id, &thread_attr, thread_func, listBack(threads_list));
			pthreadErrorHandle(err, PTHREAD_PASS, "Cannot create a thread");
		}

		// Очистка данных завершённых потоков, в зависимости от условия
		// Проходим по всем потоками, если существуют, и посылаем сигнал
		if (clearCheck(connection_count, clear_pull) && !listIsEmpty(threads_list)) {
			ListNode* threads_list_iter = threads_list->head;
			do {
				ListNode* check_item = threads_list_iter;
				threads_list_iter = threads_list_iter->next;

				// Проверяем по сигналу, завершил ли поток работу
				// Если завершил -- очищаем все данные связанные с потоком
				if (pthread_kill(((ThreadInfo*)check_item->data)->thread_id, 0)) {
					pthread_join(((ThreadInfo*)check_item->data)->thread_id, NULL);
					listDeleteNode(threads_list, check_item);
				}
			} while (threads_list_iter);
		}
	}

	pthread_attr_destroy(&thread_attr);

	listFree(&threads_list);
	close(serv_sock);
}
