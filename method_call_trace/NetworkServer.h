#ifndef _INCLUDE_NETWORK_SERVER_H_
#define _INCLUDE_NETWORK_SERVER_H_

#include <string>
#include <queue>
#include <thread>

#include <winsock2.h>
#include <mutex>


class NetworkServer
{
public:
	NetworkServer();
	~NetworkServer();
	
	void start();
	void stop();
	void enqueue_for_sending(const std::string &msg);

private:
	static void worker_proc(NetworkServer *self);
	void worker_body();
	bool init_client_connection();
	void finit_client_connection();
private:
	std::thread *m_worker;
	bool m_worker_active;
	SOCKET m_listen_socket;
	SOCKET m_client_socket;
	std::mutex m_queue_lock;
	std::queue<std::string> m_queue;
};


#endif // _INCLUDE_NETWORK_SERVER_H_