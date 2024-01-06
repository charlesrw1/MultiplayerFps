#ifndef SERVER_H
#define SERVER_H
#include "Net.h"
#include "Animation.h"
#include "Physics.h"
#include "GameData.h"
#include "Media.h"
#include "Config.h"

// If you want to add a replicated variable you must:
//	add it to entitystate or playerstate depending on its use
//	modify the ToX and FromX functions
//	modify the read/write functions that encode the state
// blech

class Model;
struct Entity
{
	EntType type = Ent_Free;
	short id = 0;

	int index = 0;
	glm::vec3 position = glm::vec3(0);
	glm::vec3 rotation = glm::vec3(0);
	float scale = 0.f;

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
	ItemState wpns;

	int model_index = 0;	// index into media.gamemodels

	Animator anim;
	const Model* model = nullptr;

	PlayerState ToPlayerState() const;
	void FromPlayerState(PlayerState* ps);
	EntityState ToEntState() const;

	void SetModel(GameModels modname);
};

Entity* ServerEntForIndex(int index);

#include "MeshBuilder.h"

class Level;
class Game
{
public:
	const static int GV_BUFFER_SIZE = 256;

	void Init();
	void ClearState();
	bool DoNewMap(const char* mapname);
	void Update();

	void SpawnNewClient(int clientnum);
	void OnClientLeave(int clientnum);

	Entity* MakeNewEntity(EntType type);
	void RemoveEntity(Entity* ent);

	void KillEnt(Entity* ent);

	void GetPlayerSpawnPoisiton(Entity* ent);
	void ExecutePlayerMove(Entity* ent, MoveCommand cmd);
	void OnPlayerKilled(Entity* ent);

	void BuildPhysicsWorld(float time);

	void ShootBullets(Entity* from, glm::vec3 dir, glm::vec3 org);

	Entity* EntForIndex(int index) {
		ASSERT(index < ents.size() && index >= 0);
		return &ents[index]; 
	}

	MeshBuilder rays;

	bool paused = false;
	float gravity = 12.f;

	PhysicsWorld phys;
	std::vector<Entity> ents;
	int num_ents = 0;
	const Level* level = nullptr;
private:
	int GetEntIndex(Entity* ent) const {
		return (ent - ents.data());
	}
	Entity* InitNewEnt(EntType type, int index);
	void BeginLagCompensation();
	void EndLagCompensation();

	short next_id = 0;
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
	void SendInitData();

	void SendReliableMsg(ByteWriter& msg);

	std::string GetIPStr() const {
		return connection.remote_addr.ToString();
	}
	float LastRecieved() const {
		return connection.last_recieved;
	}
	bool IsConnected() const {
		return state >= Connected;
	}
	bool IsSpawned() const {
		return state == Spawned;
	}

	enum ConnectionState {
		Dead,		// unused slot
		Connected,	// connected and sending initial state
		Spawned		// spawned and sending snapshots
	};
	ConnectionState state = Dead;
	Connection connection;
	float next_snapshot_time = 0.f;
	int client_num = 0;
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

	void Init();
	void Spawn(const char* mapname);
	void End();
	bool IsActive() const;
	void FixedUpdate(double dt);

	// client interface functions
	Socket* GetSock() { return &socket; }
	void RunMoveCmd(int client, MoveCommand cmd);
	void SpawnClientInGame(int client);
	void RemoveClient(int client);
	void WriteServerInfo(ByteWriter& msg);
	void WriteDeltaSnapshot(ByteWriter& msg, int deltatick, int clientnum);

	Frame* GetSnapshotFrame() {
		return &frames.at(this->tick % MAX_FRAME_HIST);
	}
	Frame* GetSnapshotFrameForTick(int tick) {
		Frame* f = &frames.at(tick % MAX_FRAME_HIST);
		if (f->tick != tick)
			return nullptr;
		return f;
	}

	bool active = false;
	std::string map_name;

	std::vector<Frame> frames;
	Frame nullframe;

	// global time var that is swapped to aid with simming client cmds
	int tick = 0;
	double simtime = 0.0;

	// CONFIG VARS
	Config_Var* cfg_snapshot_rate;
	Config_Var* cfg_tick_rate;
	Config_Var* cfg_max_time_out;
	Config_Var* cfg_sv_port;

private:
	Socket socket;
	std::vector<RemoteClient> clients;
	
	void ReadPackets();
	void UpdateClients();

	int FindClient(IPAndPort addr) const;
	void ConnectNewClient(ByteReader& msg, IPAndPort recv);
	void UnknownPacket(ByteReader& msg, IPAndPort recv);
	void BuildSnapshotFrame();
};

extern Server server;
extern Game game;

#endif // !SERVER_H
