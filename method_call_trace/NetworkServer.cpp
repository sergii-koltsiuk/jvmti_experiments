#include "NetworkServer.h"
#include <cassert>
#include <agent_util.h>
#include <mutex>

#pragma comment(lib,"ws2_32.lib") //Winsock Library


NetworkServer::NetworkServer(): 
	m_worker(nullptr), 
	m_worker_active(false),
	m_listen_socket(0), 
	m_client_socket(0)
{
}


NetworkServer::~NetworkServer()
{
}

void NetworkServer::start()
{
	if (!init_client_connection())
	{
		return;
	}

	m_worker_active = true;
	m_worker = new std::thread(&NetworkServer::worker_proc, this);
}

void NetworkServer::stop()
{
	if (m_worker_active)
	{
		assert(m_worker != NULL);

		m_worker_active = false;
		finit_client_connection();
		m_worker->join();
	}
}

void NetworkServer::enqueue_for_sending(const std::string &msg)
{
	m_queue_lock.lock();
	m_queue.push(msg);
	m_queue_lock.unlock();
}

void NetworkServer::worker_body()
{
	while (m_worker_active)
	{
		const char *message = "Hello Client , I am JVM TI\n";
		send(m_client_socket, message, strlen(message), 0);

		while (m_worker_active)
		{
			m_queue_lock.lock();
			while (!m_queue.empty())
			{
				const std::string &msg = m_queue.front();
				send(m_client_socket, msg.data(), msg.length(), 0);
				m_queue.pop();
			}
			m_queue_lock.unlock();
		}
	}
}

/*static */
void NetworkServer::worker_proc(NetworkServer *self)
{
	self->worker_body();
}



bool NetworkServer::init_client_connection()
{
	WSADATA wsa;

	struct sockaddr_in server_addr;

	stdout_message("\nInitialising Winsock...\n");
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		fatal_error("Failed. Error Code : %d\n", WSAGetLastError());
	}

	stdout_message("Initialised.\n");

	//Create a socket
	if ((m_listen_socket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
	{
		fatal_error("Could not create socket : %d\n", WSAGetLastError());
	}

	stdout_message("Socket created.\n");

	//Prepare the sockaddr_in structure
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(8888);

	//Bind
	if (bind(m_listen_socket, reinterpret_cast<struct sockaddr *>(&server_addr), sizeof(server_addr)) == SOCKET_ERROR)
	{
		fatal_error("Bind failed with error code : %d\n", WSAGetLastError());
	}

	stdout_message("Bind done\n");

	listen(m_listen_socket, 1);

	stdout_message("Waiting for incoming connections...\n");

	struct sockaddr_in client;
	int sockaddr_size = sizeof(struct sockaddr_in);
	m_client_socket = accept(m_listen_socket, reinterpret_cast<struct sockaddr *>(&client), &sockaddr_size);
	if (m_client_socket == INVALID_SOCKET)
	{
		stdout_message("accept failed with error code : %d\n", WSAGetLastError());
		return false;
	}

	stdout_message("Connection accepted\n");
	return true;
}

void NetworkServer::finit_client_connection()
{
	if (m_listen_socket != INVALID_SOCKET)
	{
		shutdown(m_listen_socket, SD_BOTH);
		closesocket(m_listen_socket);
		m_listen_socket = INVALID_SOCKET;
	}

	if (m_client_socket != INVALID_SOCKET)
	{
		shutdown(m_client_socket, SD_BOTH);
		closesocket(m_client_socket);
		m_client_socket = INVALID_SOCKET;
	}

	WSACleanup();
}