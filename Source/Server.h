#ifndef SERVER_H
#define SERVER_H
#include "Net.h"
#include "Animation.h"
#include "Physics.h"
#include "GameData.h"
#include "Config.h"
#include "Types.h"
#include "MeshBuilder.h"

// what i want:
// game_event: make particle, make sound, etc. events
// 

void GetPlayerSpawnPoisiton(Entity* ent);
void ShootBullets(Entity* from, glm::vec3 dir, glm::vec3 org);
void KillEnt(Entity* ent);
void ExecutePlayerMove(Entity* ent, Move_Command cmd);

enum Server_Client_State {
	SCS_DEAD,		// unused slot
	SCS_CONNECTED,	// connected and sending initial state
	SCS_SPAWNED		// spawned and sending snapshots
};
class Server;
class RemoteClient
{
public:
	RemoteClient(Server* sv, int slot);

	void init(IPAndPort address);
	void Disconnect(const char* log_reason);

	void OnPacket(ByteReader& msg);
	bool OnMoveCmd(ByteReader& msg);
	void OnTextCommand(ByteReader& msg);

	void Update();		// sends snapshot and/or sends reliable
	
	void SendReliableMsg(ByteWriter& msg);

	std::string GetIPStr() const {
		return connection.remote_addr.ToString();
	}
	float LastRecieved() const {
		return connection.last_recieved;
	}
	bool IsConnected() const {
		return state >= SCS_CONNECTED;
	}
	bool IsSpawned() const {
		return state == SCS_SPAWNED;
	}

	Server_Client_State state = SCS_DEAD;
	Connection connection;
	float next_snapshot_time = 0.f;
	int client_num = 0;
	bool local_client = false;
	Server* myserver = nullptr;
	int baseline = -1;	// what tick to delta encode from

	int highest_tick_recieved = 0;
	int num_commands = 0;
	vector<Move_Command> commands;
	Move_Command last_command;
};

class Server
{
public:
	static const int MAX_FRAME_HIST = 32;
	void init();	// called on engine startup
	void start();
	void end(const char* log_reason);
	void connect_local_client();

	//void WriteDeltaSnapshot(ByteWriter& msg, int deltatick, int clientnum);

	Frame* GetSnapshotFrame();
	Frame* GetSnapshotFrameForTick(int tick) {
		Frame* f = &frames.at(tick % MAX_FRAME_HIST);
		if (f->tick != tick)
			return nullptr;
		return f;
	}
	void ReadPackets();

	void make_snapshot();
	void write_delta_entities_to_client(ByteWriter& msg, int deltatick, int client);

	bool initialized = false;
	std::vector<Frame> frames;
	std::vector<RemoteClient> clients;
	Socket socket;

	// CONFIG VARS
	Config_Var* snapshot_rate;
	Config_Var* tick_rate;
	Config_Var* max_time_out;
	Config_Var* host_port;

private:
	int FindClient(IPAndPort addr) const;
	void ConnectNewClient(ByteReader& msg, IPAndPort recv);
};


#endif // !SERVER_H
