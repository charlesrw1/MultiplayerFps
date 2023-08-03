#ifndef CLIENT_H
#define CLIENT_H
#include "Net.h"
#include "Animation.h"
#include "Level.h"
#include <array>

const int NUM_TRANSFORM_ENTRIES = 8;
struct TransformEntry
{
	int tick = 0;
	glm::vec3 origin;
	glm::vec3 angles;
};
// Client side version of an entity
struct ClientEntity
{
	bool active = false;
	EntityState state;
	Animator anims;
	// position/angle history for interpolation
	std::array<TransformEntry, NUM_TRANSFORM_ENTRIES> transform_hist;
	int current_hist_index = 0;

	const Model* model = nullptr;
};

#if 0
class Client
{
public:

	void Start();
	void Quit();
	void ReadPackets();
	void SendCommands();

	void Disconnect();
	void Connect(const IPAndPort& who);
	void TrySendingConnect();

	std::vector<ClientEntity> entities;

	int connect_attempts = 0;
	double attempt_time = 0.f;

	int GetOutSequence() const { return server.out_sequence; }
	MoveCommand commands[CLIENT_MOVE_HISTORY];
	glm::vec3 view_angles = glm::vec3(0.f);		// local view angles

	bool initialized = false;
	int player_num = -1;	// what index are we
	ConnectionState state = Disconnected;
	Connection server;
private:
	bool waiting_for_first_snapshot = false;
	void DoConnect();
	void HandleUnknownPacket(const IPAndPort& addr, ByteReader& buf);
	void ReadServerPacket(ByteReader& buf);
	void ReadInitData(ByteReader& buf);
	void ReadEntitySnapshot(ByteReader& buf);
	void ReadPlayerData(ByteReader& buf);
	Socket socket;
};
#endif

enum ClientConnectionState {
	Disconnected,
	TryingConnect,	// trying to connect to server
	Connected,		// connected and receiving inital state
	Spawned,		// in server as normal
};

void ClientInit();
void ClientConnect(IPAndPort where);
void ClientDisconnect();
void ClientReconnect();
ClientConnectionState ClientGetState();
int ClientGetPlayerNum();
ClientEntity* ClientGetLocalPlayer();
bool ClientIsInGame();

class FlyCamera
{
public:
	glm::vec3 position = glm::vec3(0);
	glm::vec3 front = glm::vec3(1, 0, 0);
	glm::vec3 up = glm::vec3(0, 1, 0);
	float move_speed = 0.1f;
	float yaw = 0, pitch = 0;

	void UpdateFromInput(const bool keys[], int mouse_dx, int mouse_dy, int scroll);
	void UpdateVectors();
	glm::mat4 GetViewMatrix() const;
};

struct ViewSetup
{
	glm::vec3 vieworigin;
	glm::vec3 viewfront;
	float viewfov;
	glm::mat4 view_mat;
	glm::mat4 proj_mat;
	glm::mat4 viewproj;
	int x, y, width, height;
};

class ViewMgr
{
public:
	void Init();
	void Update();
	const ViewSetup& GetSceneView() {
		return setup;
	}

	bool third_person = false;
	bool using_debug_cam = false;

	float z_near = 0.01f;
	float z_far = 100.f;
	float fov = glm::radians(70.f);

	FlyCamera fly_cam;
	ViewSetup setup;
};

class ClientGame
{
public:
	void Init();
	void ClearState();
	void NewMap(const char* mapname);

	PlayerState player;	// local player data
	std::vector<ClientEntity> entities;
	const Level* level = nullptr;
};

class ClServerMgr
{
public:
	ClServerMgr();
	void Init();
	void Connect(const IPAndPort& port);
	void Disconnect();
	void TrySendingConnect();

	void ReadPackets();
	void SendMovesAndMessages();
private:
	void HandleUnknownPacket(IPAndPort from, ByteReader& msg);
	void HandleServerPacket(ByteReader& msg);

	void StartConnection();
	
	void ParseEntSnapshot(ByteReader& msg);
	void ParseServerInit(ByteReader& msg);
	void ParsePlayerData(ByteReader& msg);
public:
	int client_num;
	int connect_attempts;
	double attempt_time;
	ClientConnectionState state;
	Socket sock;
	Connection server;
	//LagEmulator sock_emulator;
};

class UIMgr
{

};


class Client
{
public:
	bool initialized = false;
	ViewMgr view_mgr;
	ClientGame cl_game;
	ClServerMgr server_mgr;
	UIMgr ui_mgr;

	glm::vec3 view_angles;
	std::vector<MoveCommand> out_commands;

	int tick = 0;
	double time=0.0;
public:
	MoveCommand* GetCommand(int sequence);
	int GetCurrentSequence() const;
	int GetLastSequenceAcked() const;
};
extern Client client;



#endif // !CLIENT_H
