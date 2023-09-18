#ifndef CLIENT_H
#define CLIENT_H
#include "Net.h"
#include "Animation.h"
#include "Level.h"
#include "EmulatedSocket.h"
#include "Shared.h"
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
	EntityState prev_state;
	Animator anims;

	// position/angle history for interpolation
	std::array<TransformEntry, NUM_TRANSFORM_ENTRIES> transform_hist;
	int current_hist_index = 0;

	glm::vec3 lerp_origin=glm::vec3(0.f);
	glm::vec3 lerp_angles=glm::vec3(0.f);

	const Model* model = nullptr;
};

struct Snapshot
{
	int tick = 0;				// what client tick did we receive on
	EntityState entities[16];	// keep it small for now
	PlayerState pstate;			// local player state, for prediction stuff
};

// used for prediction frames
struct PredictionState
{
	PlayerState pstate;
	EntityState estate;
};


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

	glm::vec3 interpolated_origin;		// origin to render the eye at
	PredictionState last_predicted;
	PlayerState player;	// local player data
	std::vector<ClientEntity> entities;	// client side data
	std::vector<Snapshot> snapshots;
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
	bool new_packet_arrived = false;
	int client_num;
	int connect_attempts;
	double attempt_time;
	ClientConnectionState state;
	EmulatedSocket sock;
	Connection server;
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
