#ifndef CLIENT_H
#define CLIENT_H
#include "Net.h"

#include "Level.h"
#include "EmulatedSocket.h"
#include "Client.h"
#include <array>
#include "Framework/Config.h"
#include "Types.h"
#include "Framework/MulticastDelegate.h"

struct Interp_Entry
{
	int tick = -1;
	glm::vec3 position, angles;
};

struct Entity_Interp
{
	const static int HIST_SIZE = 20;
	Interp_Entry hist[HIST_SIZE];
	int hist_index = 0;

	Interp_Entry* GetStateFromIndex(int index);
	Interp_Entry* GetLastState();
	void clear() {
		for (int i = 0; i < HIST_SIZE; i++) hist[i].tick = -1;
	}
	void increment_index() { hist_index++; hist_index %= HIST_SIZE; }
};


enum Client_State {
	CS_DISCONNECTED,
	CS_TRYINGCONNECT,	// trying to connect to server
	CS_CONNECTED,		// connected and receiving inital state
	CS_SPAWNED,			// in server as normal
};
enum class ClConnectReturn
{
	Success,			// connected succesfully
	NoResponse,			// server didnt respond to join request
	Rejected,			// server rejected our connection
};
enum class ClNetEvent
{
	ServerTimedOut,		// server conneciton timed out
	Kicked,				// server kicked us
	ServerQuit,			// server sent a quit message
	ServerChangeLevel,	// server sent a change level command
};

class Client
{
public:
	Client();

	MulticastDelegate<ClConnectReturn> on_post_connect;
	MulticastDelegate<ClNetEvent> on_net_change;

	void _init();
	void _connect(string URL);
	void _disconnect();
	void _poll();
	void _send_messages();

	void init();
	void connect(string address);
	void disconnect_from_server(const char* reason);
	void Reconnect();

	float adjust_time_step(int ticks_running);

	//void interpolate_states();
	//void run_prediction();
	//Move_Command& get_command(int sequence);

	void TrySendingConnect();

	void ReadPackets();
	void SendMovesAndMessages();

	int OutSequenceAk() const { return server.out_sequence_ak; }
	int OutSequence() const { return server.out_sequence; }
	int InSequence() const { return server.in_sequence; }

	Client_State get_state() const { return state; }
	IPAndPort GetCurrentServerAddr() const { return server.remote_addr; }

	void ForceFullUpdate() { force_full_update = true; }

	void HandleServerPacket(ByteReader& msg);
	bool OnEntSnapshot(ByteReader& msg);
	void read_entity_from_snapshot(Entity* ent, int index, ByteReader& msg, bool is_delta, 
		ByteReader* from_ent, int tick);

	int last_recieved_server_tick = 0;
	int cur_snapshot_idx = 0;

	//Frame_Storage frame_storage;

	//Entity_Interp interpolation_data[NUM_GAME_ENTS];
	//vector<Move_Command> commands;
	//vector<glm::vec3> origin_history;
	//glm::vec3 last_origin;
	//float smooth_time = 0.0;
	//int offset_debug = 0;

	int server_tick = 0;
	float time_delta = 0;

	string serveraddr;
	int client_num = -1;
	int connect_attempts;
	double attempt_time;
	Client_State state;
	EmulatedSocket sock;
	Connection server;
	bool force_full_update = false;

private:
	void disconnect_to_menu(const char* log_reason);
};


#endif // !CLIENT_H
