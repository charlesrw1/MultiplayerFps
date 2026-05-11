---@class MyApp : Application
MyApp = {
	---@type function
	on_map_callback = nil
}




function MyApp:on_post_material_load(asset)
    assert(asset~=nil)
	if asset:get_name_l()=="basicGolden.mi" then
		asset:set_physics_material(PHYS_MAT_1)
	end
end

local function setup_level()
	
end

function MyApp:start()
	PHYS_MAT_0 = make_phys_material(0.7,0.5,0.4)
	PHYS_MAT_1 = make_phys_material(0.1,0.1,1.0)

	gTimerMgr = TimerManager.new()

	GameplayStatic.change_level("my_map.tmap")
	setup_level()

	local ent = GameplayStatic.spawn_entity()
	local cam = ent:create_component(CameraComponent)
	cam:set_is_enabled(true)
	ent:set_ws_position({y=2})
end

function MyApp:pre_update()
	
end


function MyApp:update()

	local dt = GameplayStatic.get_dt()

	gTimerMgr:tick(dt)
	--Canvas.draw_text("hello world", 100,100)
	--ui_draw_2()
	LevelMgr_tick()
	if bikeMgr~=nil then
		bikeMgr:update()
	end
	if gExplosionMgr~=nil then
		gExplosionMgr:update()
	end
end
function MyApp:stop()
	
end
function MyApp:on_map_changed()
	print("MyApp::on_map_changed")
	if self.on_map_callback ~= nil then
		local saved_callback = self.on_map_callback
		self.on_map_callback = nil	-- maybe the callback will change map again so nil it here
		saved_callback()
	end
end