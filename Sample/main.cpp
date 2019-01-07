// client-server communication library
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>

typedef websocketpp::client<websocketpp::config::asio_tls_client> client;
typedef std::shared_ptr<boost::asio::ssl::context> context_ptr;
typedef websocketpp::config::asio_client::message_type::ptr message_ptr;

static context_ptr on_tls_init() {
	// establishes a SSL connection
	context_ptr ctx = std::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::sslv23);

	try {
		ctx->set_options(boost::asio::ssl::context::default_workarounds |
			boost::asio::ssl::context::no_sslv2 |
			boost::asio::ssl::context::no_sslv3 |
			boost::asio::ssl::context::single_dh_use);
	}
	catch (std::exception &e) {
		std::cout << "Error in context pointer: " << e.what() << std::endl;
	}
	return ctx;
}

// json library
#include <nlohmann/json.hpp>
using json = nlohmann::json;

// encoding and decoding in base64 library
#include "base64.h"

// standard library
#include <iostream>
#include <ctime>
#include <fstream>
#include <cmath>
#include <stdlib.h>   
#include <thread>

// program global variables
time_t timer;
struct tm ltm;
struct tm y2k = { 0 };
std::string uri;

// ----------- struct ----------/
// user's basic detail such as id
struct Connection
{
	std::string id; // unsure type
	bool registered = false;
}m_connection;

// ping-pong details
struct pingTimer {
	// default setting
	int id;
	bool enabled = true;
	int latestPing = 0;
	int PING_COUNT = 0;
	// can be loaded in with json file
	int interval = 5;
	int PING_LIMIT = 5;
	std::string logFile = "PingPongLog.txt"; 
}m_pingTimer;


// ----------- function -----------
bool loadJsonAndSetting() {
	std::ifstream ifs("setting.json");
	if (ifs.is_open()) {
		json setting = json::parse(ifs);
		uri = setting["/websocket/server"_json_pointer].get<std::string>();

		// loading setting
		m_pingTimer.interval = setting["/ping/interval"_json_pointer];
		m_pingTimer.logFile = setting["/ping/file"_json_pointer].get<std::string>();
		m_pingTimer.PING_LIMIT = setting["/ping/limit"_json_pointer];
		
		// console message
		std::cout << "connecting to server: " << uri << std::endl;
		std::cout << "set ping interval: " << m_pingTimer.interval << std::endl;;
		std::cout << "set ping limit: " << m_pingTimer.PING_LIMIT << std::endl;
		std::cout << "set ping log file: " << m_pingTimer.logFile << std::endl;

		ifs.close();

		// load for time usage
		y2k.tm_hour = 0;   y2k.tm_min = 0; y2k.tm_sec = 0;
		y2k.tm_year = 100; y2k.tm_mon = 0; y2k.tm_mday = 1;
		return true;
	}
	else {
		std::cout << "unable to loading setting" << std::endl;
		ifs.close();
		return false;
	}
}

// similiar to server-side to send to client. essentially client will need to give an appropriate message and action
bool sendMessage(client * c, websocketpp::connection_hdl m_hdl, std::string message, std::string action) {	
	// client proceed to send the action and message
	websocketpp::lib::error_code ec; // logging error message
	// action handler
	if (action == "PING") // main action to handle server-client ping pong, where client have to ping in an interval
	{
		// retrieving time
		time(&timer);  /* get current time; same as: timer = time(NULL)  */
	;
		// record last pinged time for interval ping
		m_pingTimer.latestPing = difftime(timer, mktime(&y2k));

		// sends a ping to the server with client id
		//c->ping(m_hdl, m_connection.id, ec);
		std::cout << "<< PING" << std::endl;

		message = base64_encode(reinterpret_cast<const unsigned char*>(message.c_str()), message.length());
		//std::cout << message << std::endl;
		c->send(m_hdl, message, websocketpp::frame::opcode::text, ec);
		return true;
	}
	// additional else if could be added for specific action such as 'broadcast', 'message', etc
	// ...
	// ...
	else {
		// basic message handler - encoding with base64 and sending the message. remember that message is usually in (string)json format. you can ONLY send string to server
		message = base64_encode(reinterpret_cast<const unsigned char*>(message.c_str()), message.length());
		c->send(m_hdl, message, websocketpp::frame::opcode::text, ec);
		std::cout << "<< " << action << std::endl;
		return true;
	}
	if (ec) {
		// alert user there is an error - mainly to handle user input action
		std::cout << "> Error sending message: " << ec.message() << std::endl;
	}
	return false;
}

// function ping to be used in thread
void pingingStart(client *c, websocketpp::connection_hdl m_hdl) {
	// constantly run but with a time trigger
	while (true) {

		// only ping if the client is registered and is confirmed by the server - happens during on_message and on_open
		if (m_connection.registered) {

			// already set to true at the top of the program in the configuration
			while (m_pingTimer.enabled) {

				// retrieving time
				time(&timer);

				// time trigger event 
				if (difftime(timer, mktime(&y2k)) - m_pingTimer.latestPing > m_pingTimer.interval) { // savedTime or lastPinged

					// --------------------------------------- creating message to send to server ---------------------------------------
					// load and read json for ping message - CAUTION: ping message have 125 character limit
					std::ifstream jsonFile("setting2.json");
					auto json = json::parse(jsonFile);
					auto jsonMessage = json::parse("{ \"action\": \"PING\", \"channel\": \"socketChannel\", \"type\": \"clientType\", \"message\": {\"export_data\": \"" + json["export_data"].get<std::string>() + "\", \"system\":\"something\"}, \"hd_id\": \"74657374696e67\", \"timestamp\": \"11110\"}");
					std::string s = jsonMessage.dump(4);
					//std::cout << s << std::endl;
					// sending ping and recording time sent
					sendMessage(c, m_hdl, s, "PING"); // currently ping is sending client id

					m_pingTimer.PING_COUNT++;
					// if received no pong after the limit, quit
					if (m_pingTimer.PING_COUNT > m_pingTimer.PING_LIMIT) {
						std::string placeholder;
						std::cout << ">> client is disconnected... press enter to quit" << std::endl;
						getline(std::cin, placeholder);
						exit(0);
					}
				}
			}
		}
	}
}

// server-client connection
class connection_metadata {
public:
	typedef websocketpp::lib::shared_ptr<connection_metadata> ptr;

	connection_metadata(websocketpp::connection_hdl hdl, std::string uri)
		: m_hdl(hdl)
		, m_uri(uri)
	{}

	// --------------------------------------- on open event handler ---------------------------------------
	void on_open(client * c, websocketpp::connection_hdl hdl) {

		// console message 
		std::cout << ">> you have connected to the server..." << std::endl;
		std::cout << "<< sending registration to server..." << std::endl;

		// --------------------------------------- creating message to send to server ---------------------------------------
		// generating registration in json style - could generate in string instead if desired
		auto registrationInfo = json::parse("{ \"action\": \"REGISTER\", \"channel\": \"12345\", \"type\": \"CPP\", \"message\": \"this is a register message.\", \"hw_id\": \"74657374696e67\"}");
		// encode registration
		std::string encodedMessage = base64_encode(reinterpret_cast<const unsigned char*>(registrationInfo.dump(4).c_str()), registrationInfo.dump(4).length());
		// generating registration part 2 in json style
		auto payload = json::parse("{\"hardwareId\": \"74657374696e67\", \"message\": \"" + encodedMessage + "\"}");

		// sending payload in string format!!! important
		websocketpp::lib::error_code ec;
		c->send(m_hdl, payload.dump(4), websocketpp::frame::opcode::text, ec);
		// error checking communicaton
		if (ec) {
			std::cout << ">> Error sending message: " << ec.message() << std::endl;
		}
	}

	// --------------------------------------- ping pong handler ---------------------------------------
	// client ping and waits for pong - use to verify connection
	void on_pong(client *c, websocketpp::connection_hdl hdl) {
		// retrieve timestamp
		time_t now = time(0);
		localtime_s(&ltm, &now);
		std::string timestamp = std::to_string(ltm.tm_hour) + ":" + std::to_string(ltm.tm_min) + ":" + std::to_string(ltm.tm_sec) + "\n";

		// record timestamp in local text file
		std::ofstream writeLogFile(m_pingTimer.logFile, std::ios_base::app);
		if (writeLogFile.is_open()) {
			writeLogFile << timestamp;
		}
		else {
			std::cout << "unable to log to file" << std::endl;
		}
		writeLogFile.close();
		
		// console message
		std::cout << ">> incoming pong... ";

		// retrieve time
		time(&timer);  /* get current time; same as: timer = time(NULL)  */	

		// receive time and record how long it took
		int tripTime = difftime(timer, mktime(&y2k)) - m_pingTimer.latestPing; // just as holder

		// display record
		std::cout << tripTime << "ms taken" << std::endl;

		// reset ping count
		m_pingTimer.PING_COUNT = 0;
	}
	// server ping and wait for client pong - not neccesary unless message desired. client will AUTOMATICALLY PONG back to server
	bool on_ping(client *c, websocketpp::connection_hdl hdl) {
		// retrieve timestamp
		time_t now = time(0);
		localtime_s(&ltm, &now);
		std::string timestamp = std::to_string(ltm.tm_hour) + ":" + std::to_string(ltm.tm_min) + ":" + std::to_string(ltm.tm_sec) + "\n";

		// console message
		std::cout << ">> incoming ping... on " << timestamp;
		std::cout << "\t<< pong" << std::endl;

		// sending pong
		websocketpp::lib::error_code ec; // error
		c->pong(m_hdl, m_connection.id, ec);
		if (ec) {
			std::cout << "> error sending message: " << ec.message() << std::endl;
			return true;
		}
		return false;
	}

	// handles server action messages
	bool on_message(client *c, websocketpp::connection_hdl hdl, client::message_ptr msg) {
		//std::cout << ">> incoming message: " << msg->get_payload() << std::endl;
		// doing registration first before handling any action
		if (!m_connection.registered) {
			std::string decoded = base64_decode(msg->get_payload());
			std::cout << "\t>> decoded message: " << decoded << std::endl;

			// converting to json to grab info
			auto jsonMsg = json::parse(decoded);
			m_connection.id = jsonMsg["id"].get<std::string>();


			srand(time(NULL));
			m_pingTimer.id = std::floor((std::rand() * 100) + 1);

			// console message
			std::cout << "\t>> register " << m_connection.registered << " " << m_connection.id << std::endl;
			std::cout << "\t>> socket ping-pong initialized at " << m_pingTimer.interval << "s intervals. -> " << m_pingTimer.id << std::endl;
			std::cout << ">> User Input Available - proceed to type commands" << std::endl;

			// start pinging
			m_connection.registered = true;
			return true;
		}
		else {
			if (msg->get_payload() == "pong") {
				// retrieve timestamp
				time_t now = time(0);
				localtime_s(&ltm, &now);
				std::string timestamp = std::to_string(ltm.tm_hour) + ":" + std::to_string(ltm.tm_min) + ":" + std::to_string(ltm.tm_sec) + "\n";

				// record timestamp in local text file
				std::ofstream writeLogFile(m_pingTimer.logFile, std::ios_base::app);
				if (writeLogFile.is_open()) {
					writeLogFile << timestamp;
				}
				else {
					std::cout << "unable to log to file" << std::endl;
				}
				writeLogFile.close();

				// console message
				std::cout << ">> incoming pong... ";

				// retrieve time
				time(&timer);  /* get current time; same as: timer = time(NULL)  */

				// receive time and record how long it took
				int tripTime = difftime(timer, mktime(&y2k)) - m_pingTimer.latestPing; // just as holder

				// display record
				std::cout << tripTime << "ms taken" << std::endl;

				// reset ping count
				m_pingTimer.PING_COUNT = 0;
			}
			//// always decode message as message are always encoded by the server
			//std::string decoded = base64_decode(msg->get_raw_payload());
			//// convert to json format to be used
			//auto jsonMsg = json::parse(decoded);
			//if (jsonMsg["action"].get<std::string>() == "EXIT") {
			//	std::cout << "\t>> EXIT userID: " << jsonMsg["id"] << std::endl;
			//}
			//else if (jsonMsg["action"].get<std::string>() == "NEW") {
			//	std::cout << "\t>> NEW userID: " << jsonMsg["id"] << std::endl;
			//}
			//// additional else if can be added to deal with more specific action just like in sendMessage function
			//// ...
			//// ...
			//else {
			//	std::cout << "\t>> UNKNOWN ACTION: " << jsonMsg["action"] << std::endl;
			//}
		}
		return false;
	}

	// not really used as abrupt closing, etc will be handled by the server appropriately
	void on_fail(client * c, websocketpp::connection_hdl hdl) {

		client::connection_ptr con = c->get_con_from_hdl(hdl);
		m_error_reason = con->get_ec().message();

		// error message here
		std::cout << m_error_reason << std::endl;
	}
	void on_close(client * c, websocketpp::connection_hdl hdl) {
		client::connection_ptr con = c->get_con_from_hdl(hdl);
		std::stringstream s;
		s << "close code: " << con->get_remote_close_code() << " ("
			<< websocketpp::close::status::get_string(con->get_remote_close_code())
			<< "), close reason: " << con->get_remote_close_reason();
		m_error_reason = s.str();
	}


	websocketpp::connection_hdl get_hdl() const {
		return m_hdl;
	}

private:
	websocketpp::connection_hdl m_hdl;
	std::string m_uri;
	std::string m_error_reason;
	std::vector<std::string> m_messages;
};

// client class
class websocket_endpoint {
public:
	websocket_endpoint()
	{
		m_client.clear_access_channels(websocketpp::log::alevel::all);
		m_client.clear_error_channels(websocketpp::log::elevel::all);

		m_client.init_asio();
		m_client.set_tls_init_handler(bind(&on_tls_init));
		m_client.start_perpetual();

		m_thread = websocketpp::lib::make_shared<websocketpp::lib::thread>(&client::run, &m_client);
	}
	bool connect(std::string serverURI)
	{
		// try connecting to server
		websocketpp::lib::error_code ec;
		client::connection_ptr con = m_client.get_connection(serverURI, ec);
		if (ec)
		{
			std::cout << "> Connect initialization error: " << ec.message() << std::endl;
			return false;
		}

		// event handler
		metadata_ptr = websocketpp::lib::make_shared<connection_metadata>(con->get_handle(), uri);
		con->set_open_handler(websocketpp::lib::bind(
			&connection_metadata::on_open,
			metadata_ptr,
			&m_client,
			websocketpp::lib::placeholders::_1
		));
		con->set_fail_handler(websocketpp::lib::bind(
			&connection_metadata::on_fail,
			metadata_ptr,
			&m_client,
			websocketpp::lib::placeholders::_1
		));
		con->set_close_handler(websocketpp::lib::bind(
			&connection_metadata::on_close,
			metadata_ptr,
			&m_client,
			websocketpp::lib::placeholders::_1
		));
		con->set_message_handler(websocketpp::lib::bind(
			&connection_metadata::on_message,
			metadata_ptr,
			&m_client,
			websocketpp::lib::placeholders::_1,
			websocketpp::lib::placeholders::_2
		));
		// ping-pong handler 
		con->set_pong_handler(websocketpp::lib::bind(
			&connection_metadata::on_pong,
			metadata_ptr,
			&m_client,
			websocketpp::lib::placeholders::_1
		));
		con->set_ping_handler(websocketpp::lib::bind(
			&connection_metadata::on_ping,
			metadata_ptr,
			&m_client,
			websocketpp::lib::placeholders::_1
		));
		m_client.connect(con);
		
		// start pinging
		std::thread pinging(pingingStart, &m_client, metadata_ptr->get_hdl());
		
		// allow user input
		while (true) {
			std::string input;
			getline(std::cin, input);
			std::cout << input << std::endl;
		}
	}
	~websocket_endpoint()
	{
		m_client.stop_perpetual();
		m_thread->join();
	}
private:
	client m_client;
	websocketpp::lib::shared_ptr<websocketpp::lib::thread> m_thread;
	connection_metadata::ptr metadata_ptr;
};

// ---------- main process ---------- //
int main(int argc, char* argv[]) {	

	// opening and retrieving json setting and other
	loadJsonAndSetting();

	//// not neccessary unless desired - mainly used for chat server, etc. Currently not being used, just a placeholder
	//std::string handle;
	//std::cout << "please enter your prefer name..." << std::endl;
	//getline(std::cin, handle);

	websocket_endpoint endpoint;
	endpoint.connect(uri);
	return 0;
}