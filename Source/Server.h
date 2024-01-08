#ifndef SERVER_H
#define SERVER_H
#include "Net.h"
#include "Animation.h"
#include "Physics.h"
#include "GameData.h"
#include "Media.h"
#include "Config.h"
#include "Types.h"

// If you want to add a replicated variable you must:
//	add it to entitystate or playerstate depending on its use
//	modify the ToX and FromX functions
//	modify the read/write functions that encode the state
// blech



class Model;
struct Entity
{
	const static int TRANSFORM_HIST = 8;

	int index = 0;

	// Entity State Vars
	EntType type = Ent_Free;
	glm::vec3 position = glm::vec3(0);
	glm::vec3 rotation = glm::vec3(0);
	int model_index = 0;	// index into media.gamemodels

	glm::vec3 velocity = glm::vec3(0);
	glm::vec3 view_angles = glm::vec3(0.f);
	bool ducking = false;
	bool on_ground = false;
	bool alive = true;
	bool frozen = false;
	bool in_jump = false;

	// ent specific vars
	int owner_index = 0;
	int sub_type = 0;

	float death_time = 0.0;
	int health = 100;
	Item_State items;

	void(*update)(Entity*) = nullptr;

	Animator anim;
	const Model* model = nullptr;

	bool active() { return type != Ent_Free; }
	void from_entity_state(EntityState es);
	PlayerState ToPlayerState() const;
	void FromPlayerState(PlayerState* ps);
	EntityState ToEntState() const;

	void SetModel(GameModels modname);
};

#include "MeshBuilder.h"


void GetPlayerSpawnPoisiton(Entity* ent);
void ShootBullets(Entity* from, glm::vec3 dir, glm::vec3 org);
void KillEnt(Entity* ent);
void ExecutePlayerMove(Entity* ent, MoveCommand cmd);

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

	void InitConnection(IPAndPort address);
	void Disconnect();

	void OnPacket(ByteReader& msg);
	void OnMoveCmd(ByteReader& msg);
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
};

// stores game state to delta encode to clients
struct Frame {
	static const int MAX_FRAME_ENTS = 256;
	int tick = 0;
	EntityState states[MAX_FRAME_ENTS];
	PlayerState ps_states[MAX_CLIENTS];
};
class Server
{
public:
	static const int MAX_FRAME_HIST = 32;
	void init();	// called on engine startup
	void start();
	void end();
	void connect_local_client();

	void RunMoveCmd(int client, MoveCommand cmd);
	void SpawnClientInGame(int client);
	void RemoveClient(int client);
	void WriteDeltaSnapshot(ByteWriter& msg, int deltatick, int clientnum);

	Frame* GetSnapshotFrame();
	Frame* GetSnapshotFrameForTick(int tick) {
		Frame* f = &frames.at(tick % MAX_FRAME_HIST);
		if (f->tick != tick)
			return nullptr;
		return f;
	}
	void ReadPackets();
	void BuildSnapshotFrame();

	bool initialized = false;
	std::vector<Frame> frames;
	Frame nullframe;
	std::vector<RemoteClient> clients;
	Socket socket;

	// CONFIG VARS
	Config_Var* cfg_snapshot_rate;
	Config_Var* cfg_tick_rate;
	Config_Var* cfg_max_time_out;
	Config_Var* cfg_sv_port;

private:
	int FindClient(IPAndPort addr) const;
	void ConnectNewClient(ByteReader& msg, IPAndPort recv);
};


#endif // !SERVER_H
