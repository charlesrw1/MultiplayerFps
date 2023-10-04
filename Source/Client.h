#ifndef CLIENT_H
#define CLIENT_H
#include "Net.h"
#include "Animation.h"
#include "Level.h"
#include "EmulatedSocket.h"
#include <array>

struct TransformEntry
{
	int tick = 0;
	glm::vec3 origin;
	glm::vec3 angles;
};
// Client side version of an entity
struct ClientEntity
{
	const static int TRANSFORM_HIST = 8;
	typedef std::array<TransformEntry, TRANSFORM_HIST> TransformHist;

	bool active = false;
	EntityState state;
	EntityState prev_state;

	// position/angle history for interpolation
	TransformHist transform_hist;
	int current_hist_index = 0;

	glm::vec3 lerp_origin=glm::vec3(0.f);
	glm::vec3 lerp_angles=glm::vec3(0.f);

	Animator animator;
	const Model* model = nullptr;
};

struct Snapshot
{
	int tick = 0;				// what client tick did we receive on
	EntityState entities[24];	// keep it small for now
	PlayerState pstate;			// local player state, for prediction stuff
};

enum ClientConnectionState {
	Disconnected,
	TryingConnect,	// trying to connect to server
	Connected,		// connected and receiving inital state
	Spawned,		// in server as normal
};

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
	PlayerState last_predicted;
	PlayerState player;	// local player data
	std::vector<ClientEntity> entities;	// client side data

	const Level* level = nullptr;
};

class Client;
class ClServerMgr
{
public:
	ClServerMgr();
	void Init(Client* parent);
	void Connect(const IPAndPort& port);
	void Disconnect();
	void TrySendingConnect();

	void DisableLag();
	void EnableLag(int jitter, int lag, int loss);

	void ReadPackets();
	void SendMovesAndMessages();

	int OutSequenceAk() const {
		return server.out_sequence_ak;
	}
	int OutSequence() const {
		return server.out_sequence;
	}
	int InSequence() const {
		return server.in_sequence;
	}
private:
	void HandleUnknownPacket(IPAndPort from, ByteReader& msg);
	void HandleServerPacket(ByteReader& msg);

	void StartConnection();
	
	void ParseEntSnapshot(ByteReader& msg);
	void ParseServerInit(ByteReader& msg);
public:
	bool new_packet_arrived = false;
	int client_num;
	int connect_attempts;
	double attempt_time;
	ClientConnectionState state;
	EmulatedSocket sock;
	Connection server;
	Client* myclient = nullptr;
};

class UIMgr
{

};


class Client
{
public:
	void Init();
	void Connect(IPAndPort where);
	void Disconnect();
	void Reconnect();

	ClientConnectionState GetConState() const;
	int GetPlayerNum() const;
	ClientEntity* GetLocalPlayer();
	bool IsInGame() const;

	void FixedUpdateInput(double dt);
	void FixedUpdateRead(double dt);
	void PreRenderUpdate(double dt);

	void RunPrediction();

	void CreateMoveCmd();

	void OnRecieveNewSnapshot();
public:
	bool initialized = false;
	ViewMgr view_mgr;
	ClientGame cl_game;
	ClServerMgr server_mgr;
	UIMgr ui_mgr;

	glm::vec3 view_angles;
	std::vector<MoveCommand> out_commands;

	int delta_idx = 0;
	int cur_snapshot_idx = 0;
	std::vector<Snapshot> snapshots;

	int tick = 0;
	double time=0.0;
public:
	MoveCommand* GetCommand(int sequence);
	int GetCurrentSequence() const;
	int GetLastSequenceAcked() const;

private:
	void CheckLocalServerIsRunning();
};
extern Client client;



#endif // !CLIENT_H
