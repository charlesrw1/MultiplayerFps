#ifndef CLIENT_H
#define CLIENT_H
#include "Net.h"
#include "Animation.h"
#include "Level.h"
#include "EmulatedSocket.h"
#include "Physics.h"
#include "Client.h"
#include "Particles.h"
#include <array>
#include "Config.h"
#include "Types.h"


struct Interp_Entry
{
	int tick = -1;
	glm::vec3 position, angles;
	Animator_Layer torso;
	Animator_Layer legs;
};


struct Entity_Interp
{
	const static int HIST_SIZE = 5;
	Interp_Entry hist[HIST_SIZE];
	int hist_index = 0;

	Interp_Entry* GetStateFromIndex(int index);
	Interp_Entry* GetLastState();
	void clear() {
		for (int i = 0; i < HIST_SIZE; i++) hist[i].tick = -1;
	}
};


enum Client_State {
	CS_DISCONNECTED,
	CS_TRYINGCONNECT,	// trying to connect to server
	CS_CONNECTED,		// connected and receiving inital state
	CS_SPAWNED,			// in server as normal
};

class Client
{
public:
	void init();
	void connect(string address);
	void Disconnect(const char* log_reason);
	void Reconnect();

	void interpolate_states();
	void read_snapshot(Frame* s);
	void run_prediction();
	Move_Command& get_command(int sequence);

	void TrySendingConnect();

	Frame* FindSnapshotForTick(int tick);
	Frame* GetCurrentSnapshot();

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

	Config_Var* cfg_interp_time;
	Config_Var* cfg_fake_lag;
	Config_Var* cfg_fake_loss;
	Config_Var* cfg_cl_time_out;
	Config_Var* interpolate;
	Config_Var* cfg_do_predict;
	Config_Var* smooth_error_time;
	Config_Var* dont_replicate_player;

	int last_recieved_server_tick = 0;
	int cur_snapshot_idx = 0;
	vector<Frame> snapshots;
	Entity_Interp interpolation_data[MAX_GAME_ENTS];
	vector<Move_Command> commands;
	vector<glm::vec3> origin_history;
	glm::vec3 last_origin;
	float smooth_time = 0.0;
	int offset_debug = 0;

	string serveraddr;
	int client_num = -1;
	int connect_attempts;
	double attempt_time;
	Client_State state;
	EmulatedSocket sock;
	Connection server;
	bool force_full_update = false;
};


#endif // !CLIENT_H
