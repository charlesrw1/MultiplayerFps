#ifndef SERVER_H
#define SERVER_H
#include "Net.h"

#include "Framework/Config.h"
#include "Types.h"
#include "Framework/MeshBuilder.h"

// what i want:
// game_event: make particle, make sound, etc. events
// 

void find_spawn_position(Entity* ent);
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

	int commands_ran_this_frame = 0;

	int highest_tick_recieved = 0;
	int num_commands = 0;
	//vector<Move_Command> commands;
	//Move_Command last_command;
};

class Server
{
public:
	Server();

	static const int MAX_FRAME_HIST = 32;
	void init();	// called on engine startup
	void start();
	void end(const char* log_reason);
	void connect_local_client();

	//void WriteDeltaSnapshot(ByteWriter& msg, int deltatick, int clientnum);

	Frame2* GetSnapshotFrame();
	Frame2* GetSnapshotFrameForTick(int tick) {
		Frame2& f = frame_storage.get_frame(tick);
		if (f.tick != tick)
			return nullptr;
		return &f;
	}
	void ReadPackets();

	void make_snapshot();
	void write_delta_entities_to_client(ByteWriter& msg, int deltatick, int client);

	bool initialized = false;
	Frame_Storage frame_storage;
	std::vector<RemoteClient> clients;
	Socket socket;

	// CONFIG VARS

private:
	int FindClient(IPAndPort addr) const;
	void ConnectNewClient(ByteReader& msg, IPAndPort recv);
};


#endif // !SERVER_H
