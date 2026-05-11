BIKE_ASSET = "bike.cmdl"

vec_add = lMath.vec_add
vec_sub = lMath.vec_sub
vec_mult = lMath.vec_mult
vec_multf = lMath.vec_multf
normalize = lMath.normalize
cross = lMath.cross


function create_bike_entity()
    local e = create_mesh_in_world(lMath.vec_new(0), BIKE_ASSET)
    return e
end

---@class BikeVars
BikeVars = {
    velocity=lMath.vec_new(0),
}
function BikeVars.new()
    return CopyInst(BikeVars)
end


BikeGameManager = {
    ---@type Entity
    bike_entity = nil,
    ---@type BikeVars
    vars = {},

    ---@type Entity
    camera_ent = nil,

    cur_forward_power = 0,

    cur_speed = 0,
    cur_direction = {x=0,y=0,z=1}
}
function BikeGameManager.new()
    return CopyInst(BikeGameManager)
end
function BikeGameManager:_get_camera()
    return self.camera_ent:get_component(CameraComponent)
end
function BikeGameManager:start()
    self.bike_entity = create_bike_entity()
    self.vars = BikeVars.new()

    self.camera_ent = GameplayStatic.spawn_entity()
    local theCamera = self.camera_ent:create_component(CameraComponent)
    theCamera:set_is_enabled(true)
    theCamera:set_fov(80)
    assert(CameraComponent.get_scene_camera()==theCamera)
end

function math.clamp(i,min,max)
    if i <= min then
        return min
    end
    if i >= max then
        return max
    end
    return i
end

MAX_BIKE_POWER = 5

PIVOT_OFS = 1.3
Y_DIST = PIVOT_OFS+0.5
Z_DIST = -1
CAM_DAMP = 0.02
TURN_RADIUS = 3.0

USE_TOP_DOWN_CAMERA = true

function BikeGameManager:update()
    
    local dt = GameplayStatic.get_dt()
    -- update the bike
    if lInput.was_con_button_pressed(SDL_CONTROLLER_BUTTON_DPAD_UP) then
        self.cur_forward_power = self.cur_forward_power + 1
        print("self.cur_forward_power= "..self.cur_forward_power)
    end
    if lInput.was_con_button_pressed(SDL_CONTROLLER_BUTTON_DPAD_DOWN) then
        self.cur_forward_power = self.cur_forward_power - 1
        print("self.cur_forward_power= "..self.cur_forward_power)
    end

    self.cur_forward_power = math.clamp(self.cur_forward_power, 0, MAX_BIKE_POWER)

    local turn_force = lInput.get_con_axis(SDL_CONTROLLER_AXIS_LEFTX)

    if math.abs(turn_force) < 0.2 then
        turn_force = 0.0
    end

    BikeCppUtils.debug_pre_draw_bike(self.bike_entity)
    
    local side_vec = normalize(cross(self.cur_direction,{y=1}))
    local next_dir = vec_add(self.cur_direction, vec_multf(side_vec, 1.0*TURN_RADIUS * turn_force*dt))
    self.cur_direction = normalize(next_dir)

    ---@type number
    local forward_power = self.cur_forward_power * 2
    local now_pos = self.bike_entity:get_ws_position()
    local add_vec = vec_multf(self.cur_direction,dt*forward_power)
    now_pos = vec_add(now_pos,add_vec)
   -- self.bike_entity:set_ws_position(now_pos)
    self.bike_entity:transform_look_at(now_pos,vec_sub(now_pos,self.cur_direction))

    BikeCppUtils.debug_draw_bike(self.bike_entity,0,1,5)


    -- update the camera
    local cam_pos = self.camera_ent:get_ws_position()
    local want_pos = self.bike_entity:get_ws_position()
    local offset_vec = vec_add({y=Y_DIST},vec_multf(self.cur_direction,Z_DIST))
    want_pos = vec_add(self.bike_entity:get_ws_position(), offset_vec)
    cam_pos = lMath.damp_vector(want_pos,cam_pos,CAM_DAMP,dt)
    self.camera_ent:transform_look_at(cam_pos, vec_add(self.bike_entity:get_ws_position(),{y=PIVOT_OFS}))

    if USE_TOP_DOWN_CAMERA then
        self.camera_ent:transform_look_at({y=10},{x=1,y=0})
    end


    Canvas.draw_text("want: "..want_pos.z.." have: "..cam_pos.z, 100,170)
end

