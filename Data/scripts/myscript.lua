---@class PhysicsEventCallbackImpl : IPhysicsEventCallback
PhysicsEventCallbackImpl = {
	---@type function
	callback = nil,
}
function PhysicsEventCallbackImpl:on_event(event)
	self.callback(event)
end
---@param physicsBody PhysicsBody
---@param callback function
function add_physics_callback(physicsBody, callback)
	local callbackObj = ClassBase.alloc(PhysicsEventCallbackImpl)	-- alloc here, but add_triggered_callback takes ownership
	callbackObj.callback = callback
	physicsBody:add_triggered_callback(callbackObj)
end


---@class PlayerSpawnPoint : Component
PlayerSpawnPoint = {
	only_when_damaged = false
}

---@class TdGameplayMgr : Component
TdGameplayMgr = {
	---@type Signal
	on_player_killed = nil,


	---@type PrefabAsset
	player_prefab = nil,
	spawn_points = nil,

	---@type Model export
	---@type integer export
	myInt = 0,
	---@type Entity export
	someEntity = nil,
	---@type integer export
	someBool = EASING_CONSTANT,
}

---@type TdGameplayMgr
local gGame = nil

local function init_level_0()
	local spawn0 = GameplayStatic.find_by_name("spawn0")
	if spawn0 ~= nil then
		local sp = spawn0:get_component(PlayerSpawnPoint)
		sp.only_when_damaged = true
	end
end
function TdGameplayMgr:update()
	gTimerMgr:tick(GameplayStatic.get_dt())
end

WeaponData = {
	damage = 0,
	type = 0,
	modelName = "",
	animType = 0
}
WeaponDatas = {
	RIFLE =  {
		damage = 0,
		modelName = "rifle.cmdl",
		animType = 0,
	}
}


ENTRY_LEVEL = "bike_empty.tmap"



---@class GameMgr
GameMgr = {
	spawn_points = {}
}

---@param name string
---@param callback function
function set_trigger_object_callback(name,callback)
	local e = GameplayStatic.find_by_name(name)
	if e ~=nil then
		add_physics_callback(e:get_component(PhysicsBody), callback)
	else
		print("set_trigger_object_callback: couldnt find object: "..name)
	end
end

---@class TestComponent : Component
TestComponent = {
}
function TestComponent:start()
	self:set_ticking(true)
end
function TestComponent:update()
	local v = self:get_owner():get_ws_position()
	local t = GameplayStatic.get_time()
	local yToUse = math.sin(t)
	v.y = yToUse
	self:get_owner():set_ls_position_rotation(v,Quat.from_euler(Vec3.new(0,t,0)))
end

function create_test_object(pos)
	local m = Model.load("eng/cube.cmdl")
	local e = GameplayStatic.spawn_entity():create_component(MeshComponent)
	e:set_model(m)
	e:get_owner():create_component(TestComponent)
	e:get_owner():set_ws_position(pos)
	e:get_owner():set_ls_scale(Vec3.splat(0.4))
end


function create_mesh_in_world(pos, modelName)
	local e = GameplayStatic.spawn_entity()
	local model = Model.load(modelName)
	local meshC = e:create_component(MeshComponent)
	meshC:set_model(model)
	e:set_ws_position(pos)
	return e
end

LevelMgr_ent = nil
function LevelMgr_tick()
	if LevelMgr_ent ~= nil then
		LevelMgr_ent:set_ls_euler_rotation({y=GameplayStatic.get_time()})		
	end
end
bikeMgr = nil


COUNTER = 0

function create_enemy_spawner(pos)
	local e = GameplayStatic.spawn_entity()
	e:set_ws_position(pos)
	e:create_component(EnemySpawner)
end

---@class EnemySpawner : Component
EnemySpawner = {
}
function EnemySpawner:start()
	self:_respawn()
end
function EnemySpawner:_respawn()
	self.object = create_enemy_position(self:get_owner():get_ws_position())
	local fp = self.object:get_component(FpEnemy)
	fp.on_death:add(
		function ()
			self.object = nil
			gTimerMgr:add(1.5,
				function ()
					self:_respawn()
				end
			)
		end
	)
end

---@param  callback function
---@return function
function make_trigger_once_per_actor(callback)
	local entered = {}
	---@param arg PhysicsBodyEventArg
	return function (arg)
		if arg.who~=nil then
			if entered[arg.who]==nil then
				entered[arg.who] = 0
			end
			if arg.entered_trigger then
				print("entered=",entered[arg.who])
				if entered[arg.who]==0 then
					callback(arg)
				end
				entered[arg.who] = entered[arg.who] + 1
			else
				entered[arg.who] = entered[arg.who] - 1
				if entered[arg.who]==0 then
					callback(arg)
				end
				print("left=",entered[arg.who])
			end
		end

	end
end

function setup_entry_level()
	gExplosionMgr = ExplosionMgr.new()


	print("setup_entry_level")
	LevelMgr_ent=nil

	set_trigger_object_callback("doortrigger0",function (arg)
		COUNTER = COUNTER + 1
		--if arg.entered_trigger then
			--print("entered_trigger")
		--else
		--	print("left trigger")
		--end
	end)

	--local enter_exit_count = 0
	-----@param arg PhysicsBodyEventArg
	--set_trigger_object_callback("tetris_trigger",function (arg)
	--	local p = arg.who:get_component(FpPlayer)
	--	if p==nil then
	--		return
	--	end
	--	if arg.entered_trigger then
	--		enter_exit_count = enter_exit_count + 1
	--		print("entered trigger:"..enter_exit_count)
	--		if enter_exit_count == 1 then
	--			p.weapon_data:switch_to(WEAPON_C4)
	--		end
	--	else
	--		enter_exit_count = enter_exit_count - 1
	--		print("exited trigger"..enter_exit_count)
	--	end
	--end)
	set_trigger_object_callback("tetris_trigger",make_trigger_once_per_actor(function (arg)
		local p = arg.who:get_component(FpPlayer)
		if p ~= nil then
			if arg.entered_trigger then
				p.weapon_data:switch_to(WEAPON_C4)
			end
		end

	end))

	--local ent = GameplayStatic.find_by_name("2620")
	
	--ent.weapon_type = WEAPON_PHYSICS


	--GameplayStatic.find_by_name("doortrigger0"):create_component(TestComponentCpp)

	LevelMgr_ent= create_mesh_in_world({},"cylinder_nose.cmdl")
	--bikeMgr = BikeGameManager.new()
	--bikeMgr:start()

	GameplayStatic:spawn_entity():create_component(FpPlayer)


	assert(LevelMgr_ent~=nil)
	--for i = 1, 100, 1 do
	--	create_test_object({x=i})
	--end
	create_enemy_spawner({x=5})
	create_enemy_spawner({x=5,z=1})
	create_enemy_spawner({x=5,z=2})

	local ragdollPfb = PrefabAsset.load("ragdoll.pfb")

	local created = GameplayStatic.spawn_prefab(PrefabAsset.load("ragdoll.pfb"))
	created:set_ws_position({x=1})

end


COUNTER_X = 0
COUNTER_Y = 0

---@param meshComponent MeshComponent
---@param callback function
function create_and_set_animator_tree(meshComponent, callback)
	local builder = ClassBase.alloc(agBuilder)
	callback(builder)
	meshComponent:create_animator(builder)
	ClassBase.free(builder)
end

---@return Entity
function create_player_object()
	local e = GameplayStatic.spawn_entity()
	local playerMod = Model.load("characters/swat_model/swat_model.cmdl")
	local mesh = e:create_component(MeshComponent)
	mesh:set_model(playerMod)
	return e
end

function ui_draw_2()
	local s = GameplayStatic.get_current_level_name()

	Canvas.draw_text("level name: "..s, 100,130)
	Canvas.draw_text("x:"..COUNTER, 100,150)

	local str = "controller not connected"
	if lInput.is_any_con_active() then
		str = "controller connected"
	end
	Canvas.draw_text(str, 100,170)

	if lInput.was_key_released(SDL_SCANCODE_0) then
	end
end





function TdGameplayMgr:_spawn_player()
	local p = GameplayStatic.spawn_entity()
end


function TdGameplayMgr:pre_start()
	self:set_ticking(true)
	gGame = self
	gTimerMgr = TimerManager.new()

--asd
	--print_table("blah")
	self.on_player_killed = Signal.new()

	self.on_player_killed:add(function (arg)
		print("on_player_killed: "..arg.name)
	end)

	self.timer_mgr = TimerManager.new()
	self.spawn_points = GameplayStatic.find_components(PlayerSpawnPoint)
	if next(self.spawn_points) == nil then
	else
		---@type PlayerSpawnPoint
		local c = self.spawn_points[1]
		local pos = c:get_owner():get_ws_position()
		PrintTable(pos)
		pos = pos + Vec3.new(0,2,0)
	end
	init_level_0()
	print("Global game start")
	gGame.timer_mgr:add(5.0,function ()
		print("callback!")
		---@type TopDownPlayer[]
		local find = GameplayStatic.find_components(TopDownPlayer)
		for index, value in ipairs(find) do
		--	value:get_owner():destroy_deferred()
		end
		self.on_player_killed:invoke({name="BRUH!"})

	end)
end
function TdGameplayMgr:start()
	
end
function TdGameplayMgr:stop()
	gGame = nil
	gTimerMgr = nil
	print("Global game end")
end




---@class PlayerAgFactoryImpl : PlayerAgFactory
PlayerAgFactoryImpl = {
}


---@param model Model
---@param out agBuilder
function PlayerAgFactoryImpl:create(model,out)
	print("PlayerAgFactoryImpl:create")

	local clipRoot = out:alloc(agClipNode)
	clipRoot:set_clip(model,"run_forward_unequip")
	clipRoot:set_looping(true)

	local clipTurnL = out:alloc(agClipNode)
	clipTurnL:set_clip(model,"turn_left")

	local clipTurnR = out:alloc(agClipNode)
	clipTurnR:set_clip(model,"turn_right")

	--local clipStand = out:alloc(agClipNode)
	--clipStand:set_clip(model,"stand_rifle_aim_l")

	local blend = out:alloc(agBlendByInt)
	blend:set_transition_data(EASING_CUBICEASEOUT,0.8)
	blend:set_integer_var("iState")
	blend:append_input(clipRoot)
	blend:append_input(clipTurnL)
	blend:append_input(clipTurnR)

	local runClip = out:alloc(agClipNode)
	runClip:set_clip(model,"run_forward_unequip")
	runClip:set_looping(true)

	local maskedBlend = out:alloc(agBlendMasked)
	maskedBlend:init_mask_for_model(model,0.0)
	maskedBlend:set_all_children_weights(model,"mixamorig:Spine2",1.0)
	maskedBlend:set_one_bone_weight(model,"mixamorig:Spine2",0.6)
	maskedBlend:set_one_bone_weight(model,"mixamorig:Spine1",0.4)
	maskedBlend:set_one_bone_weight(model,"mixamorig:Spine",0.1)
	maskedBlend:set_inputs(runClip, blend)
	maskedBlend:set_meshspace_blend(true)
	maskedBlend:set_alpha_const(0.9)

	--blend:set_integer_var()

	out:set_root(maskedBlend)
end

---@class TestClassI : InterfaceClass
TestClassI = {
	myInt = 98
}

function TestClassI:get_value(str)
	local result = GameplayStatic.cast_ray()
	local dotRes = result.pos:dot(Vec3.new(0.4,0,0))
	if result.hit then
		print("hit! " .. dotRes)
	end
	result.pos = {x=0,y=1,z=2}
	GameplayStatic.send_back_result(result)

	StaticClass.get_class():func()
	StaticClass.do_something()
	print(str)
	self:set_str(str)
	local b =  self:is_subclass_of(InterfaceClass)

	if b then
		return 1
	else
		return 0
	end
end

function TestClassI:buzzer()
	self:set_var(self.myInt)
	print(self:self_func())
end