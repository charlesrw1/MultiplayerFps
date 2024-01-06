#ifndef CLIENT_H
#define CLIENT_H
#include "Net.h"
#include "Animation.h"
#include "Level.h"
#include "EmulatedSocket.h"
#include "Physics.h"
#include "Client.h"
#include "Particles.h"
#include <array>
#include "Config.h"
#include "Types.h"

struct StateEntry
{
	int tick = 0;
	EntityState state;
};

struct ClientEntity
{
	const static int NUM_STORED_STATES = 25;
	typedef std::array<StateEntry, NUM_STORED_STATES> StateHist;

	short id = 0;
	bool active = false;

	// history of updates for interpolation
	StateHist hist;
	int current_hist_index = 0;
	EntityState interpstate;

	Animator animator;
	const Model* model = nullptr;

	StateEntry* GetStateFromIndex(int index);
	StateEntry* GetLastState();
	void OnRecieveUpdate(const EntityState* state, int tick);
	void InterpolateState(double time, double tickrate);
	void ClearState() {
		for (int i = 0; i < hist.size(); i++)
			hist.at(i) = { -1, {} };
		model = nullptr;
		animator.Clear();
	}
};

struct Snapshot
{
	static const int MAX_ENTS = 256;

	int tick = 0;				// what client tick did we receive on
	EntityState entities[MAX_ENTS];	// keep it small for now
	PlayerState pstate;			// local player state, for prediction stuff
};

enum ClientConnectionState {
	Disconnected,
	TryingConnect,	// trying to connect to server
	Connected,		// connected and receiving inital state
	Spawned,		// in server as normal
};
class ClientGame
{
public:
	ClientGame();
	void Init();

	//void ClearState();
	//void NewMap(const char* mapname);
	
	void ComputeAnimationMatricies();
	void InterpolateEntStates();

	void RunCommand(const PlayerState* in, PlayerState* out, MoveCommand cmd, bool run_fx);

	//void PreRenderUpdate();	// update client side stuff

	ClientEntity* EntForIndex(int index) {
		ASSERT(index >= 0 && index < entities.size());
		return &entities[index];
	}

	//void BuildPhysicsWorld();

	Random rand;	// only use for client-side effects
	//PhysicsWorld phys;
	ParticleMgr particles;

	std::vector<ClientEntity> entities;	// client side data
public:

};

class Client;
class ClServerMgr
{
public:
	ClServerMgr();
	void Init(Client* parent);

	void force_into_local_game() {
		client_num = 0;
		state = Spawned;
		server.Init(&sock, IPAndPort());
	}

	void Connect(const IPAndPort& port);
	void Disconnect();
	void TrySendingConnect();

	void DisableLag();
	void EnableLag(int jitter, int lag, int loss);

	void ReadPackets();
	void SendMovesAndMessages();

	int OutSequenceAk() const { return server.out_sequence_ak; }
	int OutSequence() const { return server.out_sequence; }
	int InSequence() const { return server.in_sequence; }
	int ClientNum() const { return client_num; }
	ClientConnectionState GetState() const { return state; }
	IPAndPort GetCurrentServerAddr() const { return server.remote_addr; }

	void ForceFullUpdate() {
		force_full_update = true;
	}
private:
	void HandleUnknownPacket(IPAndPort from, ByteReader& msg);
	void HandleServerPacket(ByteReader& msg);

	void StartConnection();
	
	bool OnEntSnapshot(ByteReader& msg);
	void OnServerInit(ByteReader& msg);
private:
	int client_num;
	int connect_attempts;
	double attempt_time;
	ClientConnectionState state;
	EmulatedSocket sock;
	Connection server;
	Client* myclient = nullptr;
	bool force_full_update = false;
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

	void ClearEntsThatDidntUpdate(int what_tick);
	void SetupSnapshot(Snapshot* s);
	MoveCommand* GetCommand(int sequence);
	int GetCurrentSequence() const;
	int GetLastSequenceAcked() const;
	void SetNewTickRate(float tick_rate);

	Snapshot* FindSnapshotForTick(int tick);
	Snapshot* GetCurrentSnapshot();

	ClientGame cl_game;

	int last_recieved_server_tick = 0;
	int cur_snapshot_idx = 0;
	std::vector<Snapshot> snapshots;
	PlayerState lastpredicted;
	ClientEntity interpolation_data[MAX_GAME_ENTS];


	int tick = 0;
	double time=0.0;

	// CONFIG VALS
	Config_Var* cfg_interp_time;
	Config_Var* cfg_fake_lag;
	Config_Var* cfg_fake_loss;
	Config_Var* cfg_cl_time_out;
	Config_Var* cfg_mouse_sensitivity;


	void ForceFullUpdate() { server_mgr.ForceFullUpdate(); }
	ClServerMgr server_mgr;
	void RunPrediction();
private:
	void CreateMoveCmd();
	void DoViewAngleUpdate();
	void CheckLocalServerIsRunning();

};


#endif // !CLIENT_H
