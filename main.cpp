// -----------------------------
#include <cstring>
#include <iostream>
#include <asio.hpp>
// #include <cstdlib>
#include <thread>
#include <chrono>
#include <system_error>
// #include "usage.hpp"
#include "general\circular_buffer.hpp"
#include "general\varlen.hpp"
// -----------------------------

int main(int argc, char* argv[])
{

	try
	{
		using asio::ip::tcp;
		using asio::buffer;

		if (argc != 3) {
			std::cerr << "Usage: Connector <host> <port>"; return 1;
		}

		// buffers
		circullar_buffer send_buf;
		circullar_buffer recv_buf;

		int bytes_sent = 0;
		int bytes_readen = 0;

		// constructing packets
		// handshake
		varint handshake_len;
		varint handshake_ID = 0;
		varint protocol_ver = -1; // should be -1 for noclient convention // 340 = 1.12.2
		varint hostname_len = strlen(argv[1]);
		char* hostname_ptr = argv[1];
		unsigned short port = atoi(argv[2]);
		varint nxt_state = 0x01;

		handshake_len = 0;
		handshake_len = handshake_ID.len() + protocol_ver.len() +
			hostname_len.len() + hostname_len + sizeof(port) + nxt_state.len();

		send_buf << handshake_len << handshake_ID << protocol_ver << hostname_len;
		send_buf.putn(hostname_ptr, hostname_len);
		send_buf << port << nxt_state;

		// request : len, ID, nodata
		send_buf << varint(1) << varint(0);

		// ping pong : len, ID, payload
		send_buf << varint(9) << varint(1) << (long long)0;

		// connection
		asio::io_service io_service;
		tcp::resolver resolver(io_service);
		tcp::resolver::query query(argv[1], argv[2]);
		tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
		tcp::socket socket(io_service);
		asio::connect(socket, endpoint_iterator);
		std::error_code error;

		char* data = send_buf.get_linear_readable_buffer_pointer();
		int   size = send_buf.get_linear_readable_buffer_size();
		asio::write(socket, asio::buffer(data, size), error);
		send_buf.data_was_readen_from_buffer(data, size);
		bytes_sent += size;

		int bytes_prev;

		// for (;;)
		// {
		    std::this_thread::sleep_for( std::chrono::milliseconds(1000) );
			bytes_prev = bytes_readen;

			data = recv_buf.get_linear_writable_buffer_pointer();
			size = recv_buf.get_linear_writable_buffer_size();
			auto buf = asio::buffer(data, size);

			size = socket.read_some(buf, error);
			bytes_readen += size;
			recv_buf.data_was_written_into_buffer(data, size);

			if (error == asio::error::eof)
            {
                std::cout << "EOF has been met!" << '\n';
                return 1;
            }
			else if (error) throw std::system_error(error);
		// }

		// packet parsing not done yet
		varint packet_len, packet_ID;
		recv_buf >> packet_len >> packet_ID;

		if (packet_ID != 0)
		{
			std::cout << "Something different than Response packet recieved!\n";
			return 1;
		}

		char* JSON;
		varint json_len;
		recv_buf >> json_len;
		JSON = new char[json_len + 1];
		JSON[json_len] = '\0';
		recv_buf.getn(JSON, json_len);

		long long pong;
		bool ping_pong_OK = false;
		recv_buf >> packet_len >> packet_ID >> pong;
		if (pong == 0) ping_pong_OK = true;

		// displaying results
		std::cout << JSON << '\n' << '\n'
			<< "\tping pong is:\t" << (ping_pong_OK ? "OK!\n" : "broken!\n")
			<< "\tbytes sent:\t" << bytes_sent << '\n'
			<< "\tbytes readen:\t" << bytes_readen << '\n';

		if (recv_buf.size())
		{
			std::cout << "\tremaining data in recv_buf: " << recv_buf.size() << ' ' << "bytes\n";

		}
	}
	catch (std::exception& e)
	{
		std::cerr << e.what() << std::endl;
	}
	catch (const char* str)
	{
	    std::cerr << str;
	}

	return 0;
}
