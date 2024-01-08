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

struct StateEntry
{
	int tick = -1;
	EntityState state;
};

struct ClientEntity
{
	const static int NUM_STORED_STATES = 10;
	typedef std::array<StateEntry, NUM_STORED_STATES> StateHist;

	// history of updates for interpolation
	StateHist hist;
	int hist_index = 0;
	EntityState interpstate;

	StateEntry* GetStateFromIndex(int index);
	StateEntry* GetLastState();
	void ClearState() {
		for (int i = 0; i < hist.size(); i++)
			hist.at(i) = { -1, {} };
	}
};

struct Snapshot
{
	static const int MAX_ENTS = 256;

	int tick = 0;				// what client tick did we receive on
	EntityState entities[MAX_ENTS];	// keep it small for now
	PlayerState pstate;			// local player state, for prediction stuff
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
	void Disconnect();
	void Reconnect();

	void interpolate_states();
	void read_snapshot(Snapshot* s);
	void run_prediction();
	MoveCommand& get_command(int sequence);

	void TrySendingConnect();
	int GetPlayerNum() const;
	void ClearEntsThatDidntUpdate(int what_tick);

	Snapshot* FindSnapshotForTick(int tick);
	Snapshot* GetCurrentSnapshot();

	void ReadPackets();
	void SendMovesAndMessages();

	int OutSequenceAk() const { return server.out_sequence_ak; }
	int OutSequence() const { return server.out_sequence; }
	int InSequence() const { return server.in_sequence; }

	int ClientNum() const { return client_num; }
	Client_State get_state() const { return state; }
	IPAndPort GetCurrentServerAddr() const { return server.remote_addr; }

	void ForceFullUpdate() { force_full_update = true; }

	void HandleServerPacket(ByteReader& msg);
	bool OnEntSnapshot(ByteReader& msg);

	Config_Var* cfg_interp_time;
	Config_Var* cfg_fake_lag;
	Config_Var* cfg_fake_loss;
	Config_Var* cfg_cl_time_out;

	int last_recieved_server_tick = 0;
	int cur_snapshot_idx = 0;
	vector<Snapshot> snapshots;
	PlayerState lastpredicted;
	ClientEntity interpolation_data[MAX_GAME_ENTS];
	vector<MoveCommand> commands;

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
