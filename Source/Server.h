#ifndef SERVER_H
#define SERVER_H
#include "Net.h"
#include "Animation.h"
#include "Physics.h"
#include "GameData.h"

// If you want to add a replicated variable you must:
//	add it to entitystate or playerstate depending on its use
//	modify the ToX and FromX functions
//	modify the read/write functions that encode the state
// blech

class Model;
struct Entity
{
	EntType type = Ent_Free;
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
	WpnState wpns;

	Animator anim;
	const Model* model = nullptr;

	PlayerState ToPlayerState() const;
	void FromPlayerState(PlayerState* ps);
	EntityState ToEntState() const;
};

Entity* ServerEntForIndex(int index);

#include "MeshBuilder.h"

class Level;
class Game
{
public:
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

	void RayWorldIntersect(Ray r, RayHit* out, int skipent, PhysFilterFlags filter);
	void PhysWorldTrace(PhysContainer obj, GeomContact* contact, int skipent, PhysFilterFlags filter);
	void ShootBullets(Entity* from, glm::vec3 dir, glm::vec3 org);

	Entity* EntForIndex(int index) {
		ASSERT(index < ents.size() && index >= 0);
		return &ents[index]; 
	}

	MeshBuilder rays;

	bool paused = false;
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
};

struct RemoteClient {
	enum ConnectionState {
		Dead,		// unused slot
		Connected,	// connected and sending initial state
		Spawned		// spawned and sending snapshots
	};
	ConnectionState state = Dead;
	Connection connection;
	float next_snapshot_time = 0.f;
};

// stores game state to delta encode to clients
struct Frame {
	static const int MAX_FRAME_ENTS = 64;
	int tick = 0;
	EntityState states[MAX_FRAME_ENTS];
	PlayerState ps_states[MAX_CLIENTS];
};

class Server;
class ClientMgr
{
public:
	ClientMgr();
	void Init();

	void ReadPackets();
	void SendSnapshots();
	void ShutdownServer();

	std::vector<RemoteClient> clients;

private:
	Socket socket;
	Server* myserver = nullptr;
	Frame nullframe;

	int FindClient(IPAndPort who) const;
	int GetClientIndex(const RemoteClient& client) const;

	void HandleUnknownPacket(IPAndPort from, ByteReader& msg);
	void HandleClientPacket(RemoteClient& from, ByteReader& msg);

	void ParseClientMove(RemoteClient& from, ByteReader& msg);
	void ParseClientText(RemoteClient& from, ByteReader& msg);

	void SendSnapshotUpdate(RemoteClient& to);
	void WriteDeltaSnapshot(Frame* from, Frame* to, ByteWriter& msg, int client_index);


	void DisconnectClient(RemoteClient& client);
	void ConnectNewClient(IPAndPort who, ByteReader& msg);
	void SendInitData(RemoteClient& client);
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

	Frame* GetLastSnapshotFrame() {
		int i = cur_frame_idx - 1;
		if (i < 0) i += MAX_FRAME_HIST;
		return &frames.at(i);
	}

	bool active = false;
	std::string map_name;
	ClientMgr client_mgr;
	Game sv_game;

	int cur_frame_idx = 0;
	std::vector<Frame> frames;

	int tick = 0;

	// global time var that is swapped to aid with simming client cmds
	double simtime = 0.0;

private:
	void BuildSnapshotFrame();
};

extern Server server;

#endif // !SERVER_H
