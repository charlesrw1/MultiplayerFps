#ifndef SERVER_H
#define SERVER_H
#include "Net.h"

enum EntType
{
	Ent_Player,
	Ent_Dummy,
	Ent_Free = 0xff,
};

class Model;
struct Entity
{
	EntType type = Ent_Free;
	int index = 0;
	glm::vec3 position = glm::vec3(0);
	glm::vec3 rotation = glm::vec3(0);
	float scale = 0.f;
	const Model* model = nullptr;
	int animation_num = 0;
	float animation_time = 0.f;
	int gun_type = 0;

	glm::vec3 velocity = glm::vec3(0);
	glm::vec3 view_angles = glm::vec3(0.f);
	bool ducking = false;
	bool on_ground = false;
};

void ServerInit();
void ServerQuit();
void ServerSpawn(const char* mapname);
bool ServerIsActive();
Entity* ServerEntForIndex(int index);

class Level;
class Game
{
public:
	void Init();
	void ClearAllEnts();
	bool DoNewMap(const char* mapname);
	void SpawnNewClient(int clientnum);
	int MakeNewEntity(EntType type, glm::vec3 pos, glm::vec3 rot);
	void GetPlayerSpawnPoisiton(Entity* ent);
	void ExecutePlayerMove(Entity* ent, MoveCommand cmd);

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

class ClientMgr
{
public:
	void Init();
	void ReadPackets();
	void SendSnapshots();
	void ShutdownServer();

	std::vector<RemoteClient> clients;
	Socket socket;

private:
	int FindClient(IPAndPort who) const;
	int GetClientIndex(const RemoteClient& client) const;

	void HandleUnknownPacket(IPAndPort from, ByteReader& msg);
	void HandleClientPacket(RemoteClient& from, ByteReader& msg);

	void ParseClientMove(RemoteClient& from, ByteReader& msg);
	void ParseClientText(RemoteClient& from, ByteReader& msg);

	void SendSnapshotUpdate(RemoteClient& to);

	void DisconnectClient(RemoteClient& client);
	void ConnectNewClient(IPAndPort who, ByteReader& msg);
	void SendInitData(RemoteClient& client);
};

class Server
{
public:
	bool active = false;
	std::string map_name;
	ClientMgr client_mgr;
	Game sv_game;

public:
	void Init();
	void Shutdown();
	int FindClient(const IPAndPort& addr) const;
};

extern Server server;

#endif // !SERVER_H
