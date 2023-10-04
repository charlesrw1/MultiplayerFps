#ifndef SERVER_H
#define SERVER_H
#include "Net.h"
#include "Animation.h"

class Model;
struct Entity
{
	EntType type = Ent_Free;
	int index = 0;
	glm::vec3 position = glm::vec3(0);
	glm::vec3 rotation = glm::vec3(0);
	float scale = 0.f;
	int gun_type = 0;

	float next_shoot_time = -100.0f;
	glm::vec3 velocity = glm::vec3(0);
	glm::vec3 view_angles = glm::vec3(0.f);
	bool ducking = false;
	bool on_ground = false;
	bool alive = true;

	const Model* model = nullptr;
	Animator anim;

	PlayerState ToPlayerState() const;
	void FromPlayerState(PlayerState* ps);
	EntityState ToEntState() const;
};

Entity* ServerEntForIndex(int index);

class Level;
class Game
{
public:
	void Init();
	void ClearState();
	bool DoNewMap(const char* mapname);
	void SpawnNewClient(int clientnum);
	void OnClientLeave(int clientnum);

	int MakeNewEntity(EntType type, glm::vec3 pos, glm::vec3 rot);
	void GetPlayerSpawnPoisiton(Entity* ent);
	void ExecutePlayerMove(Entity* ent, MoveCommand cmd);

	bool paused = false;
	std::vector<Entity> ents;
	int num_ents = 0;
	const Level* level = nullptr;
private:
	Entity* InitNewEnt(EntType type, int index);
};

struct RemoteClient {
	enum ConnectionState {
		Dead,		// unused slot
		Connected,	// connected and sending initial state
		Spawned		// spawned and sending snapshots
	};
	ConnectionState state = Dead;
	Connection connection;
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
	
	Entity* EntForIndex(int index);

	void FixedUpdate(double dt);

	bool active = false;
	std::string map_name;
	ClientMgr client_mgr;
	Game sv_game;


	Frame* GetLastSnapshotFrame() {
		int i = cur_frame_idx - 1;
		if (i < 0) i += MAX_FRAME_HIST;
		return &frames.at(i);
	}

	int cur_frame_idx = 0;
	std::vector<Frame> frames;

	int tick = 0;
	double time = 0.0;
public:
	int FindClient(const IPAndPort& addr) const;

private:
	void BuildSnapshotFrame();
};

extern Server server;

#endif // !SERVER_H
