

---@class FpPlayer : Component
FpPlayer = {
}

---@class LightFlicker : Component
LightFlicker = {
    color = {},
    intensity = 1
}
function LightFlicker:start()
    self.light = self:get_owner():get_component(PointLightComponent)
    self:set_ticking(true)
end
PERIOD = 20
function LightFlicker:update()
    local v = math.sin(GameplayStatic.get_time()*PERIOD)
    v = v*0.5 + 0.5
    self.light:set_color(self.color, v*self.intensity)
end

---@class ExplosionMgr
---@field material MaterialInstance
ExplosionMgr = {
    explosions = {}
}
EXPLOSION_LEN = 1.5
function ExplosionMgr.new()
    local out = CopyInst(ExplosionMgr)
    out.material = MaterialInstance.load("explosion_sphere.mm")
    return out
end
function ExplosionMgr:update()
    local timeNow = GameplayStatic.get_time()
    --local remove_these = {}

    local next_list = {}
    for index, value in ipairs(self.explosions) do
        local duration = timeNow-value.time
        if duration > EXPLOSION_LEN then
           -- remove_these[#remove_these+1] = index
            MaterialInstance.free_dynamic_mat(value.material)
            ---@type Entity
            local theEnt = value.entity
            theEnt:destroy()
        else
            ---@type MaterialInstance
            local dynMat = value.material
            dynMat:set_float_parameter("Time",duration/EXPLOSION_LEN)

            next_list[#next_list+1]=value
        end
    end
    self.explosions = next_list
   
end
function ExplosionMgr:create_explosion(pos)
    local dynMat = MaterialInstance.alloc_dynamic_mat(self.material)
    dynMat:set_float_parameter("Time",0.0)
    local ent = GameplayStatic.spawn_entity()
    ent:set_ws_position(pos)
    ent:set_ls_scale(lMath.vec_new(2.0))
    local mesh = ent:create_component(MeshComponent)
    mesh:set_model(Model.load("sphere.cmdl"))
    mesh:set_material_override(dynMat)
    self.explosions[#self.explosions+1] = {entity=ent,material=dynMat,time=GameplayStatic.get_time()}
end


---@class Grenade : Component
Grenade = {
    spawn_time = 0.0,
}
function Grenade:start()
    --local body = self:get_owner():get_component(PhysicsBody)
    --assert(body~=nil)
    self:set_ticking(true)
    self.spawn_time = GameplayStatic.get_time()
end

function lerp(min,max,alpha)
    return (max-min)*alpha+min
end

---@param pos lVec3
---@param player FpPlayer
function explode_shared(pos,player)
    gExplosionMgr:create_explosion(pos)
    
    --GameplayStatic.play_spatial_sound(pos,SoundFile.load("top_down/shotgun2.wav"),3,50,SNDATN_CUBIC)

    ---@type Entity[]
    local objs = GameplayStatic.sphere_overlap(pos,3.0,GameplayStatic.get_collision_mask_for_physics_layer(PL_PHYSICSOBJECT))
    for index, value in ipairs(objs) do
        local body = value:get_component(PhysicsBody)
        if body~=nil and (body:get_body_type() ~= BODYTYPE_STATIC) then
            local dir = normalize(vec_sub(body:get_owner():get_ws_position(),pos))
            
            body:apply_impulse(pos,vec_multf(dir,GRANDE_EXPLODE_FORCE))
        end
    end
    
    if player~=nil then
        
        local dist = lMath.length(vec_sub(player:get_owner():get_ws_position(), pos))
        print("dist="..dist)
        print("me=")
        PrintTable(pos)
        print("player=")
        PrintTable(player:get_owner():get_ws_position())
        local max_dist = 20
        if dist < max_dist then
            local alpha = 1.0-dist/max_dist
            local intensity = lerp(0.005,0.15,alpha)
            local duration = lerp(0.02,0.25,alpha)
            player.cam_shake:start_shake(intensity,duration)
        end
    end
end

GRENADE_LIFE = 2.0
GRANDE_EXPLODE_FORCE = 100.0
function Grenade:_explode()
    local pos = self:get_owner():get_ws_position()
    explode_shared(pos)
    self:get_owner():destroy_deferred()
end
function Grenade:update()
    local pos = self:get_owner():get_ws_position()
    ---@type Entity[]
    local objs = GameplayStatic.sphere_overlap(pos,0.5,GameplayStatic.get_collision_mask_for_physics_layer(PL_CHARACTER))
    if #objs > 1 then
        self:_explode()
    end
    local timeNow = GameplayStatic.get_time()
    if self.spawn_time+GRENADE_LIFE<timeNow then
        self:_explode()
    end

    for index, value in ipairs(objs) do
        local enemyFind = value:get_component(FpEnemy)
        if enemyFind~=nil then
            enemyFind:kill()
        end
    end
end




function controller_dead_zone(val, min)
    if math.abs(val) < min then
        return 0
    end    
    return val
end
DEADZONE_VAL = 0.2
SENSITIVITY = 0.1
PLAYER_HEIGHT = 1.7
MOVE_SPEED = 5.0
CROSSHAIR_SIZE = 50


function FpPlayer:_draw_player_ui()
    
    local crosshair_tex = Texture.load("crosshair.png")

    local r = Canvas.get_window_rect()
    local out_rect = {}

    out_rect.x = r.w/2-CROSSHAIR_SIZE*0.5
    out_rect.y = r.h/2-CROSSHAIR_SIZE*0.5
    out_rect.w = CROSSHAIR_SIZE
    out_rect.h = CROSSHAIR_SIZE

    Canvas.draw_rect(out_rect,crosshair_tex,{})
end
SHOOTCOOLDOWN = 0.15
IMPULSE_STR = 80

function FpPlayer:_shoot_hitscan()
    local vpos,vdir = self:_get_view_pos_and_dir()
    local visMask = GameplayStatic.get_collision_mask_for_physics_layer(PL_VISIBLITY)
    local hitresult = GameplayStatic.cast_ray(vpos,vec_add(vpos,vec_multf(vdir,10.0)),visMask, nil)
    if hitresult.hit then
        print("HIT")
        local body = hitresult.what:get_component(PhysicsBody)
        if body ~= nil and body:get_body_type() == BODYTYPE_DYNAMIC then
            print("applied impulse")
            body:apply_impulse(hitresult.what:get_ws_position(),vec_multf(vdir,IMPULSE_STR))
            gExplosionMgr:create_explosion(hitresult.pos)
        end
        
    else  
        print("no hit")
    end
end

GRENADE_IMPULSE = 1.3





function FpPlayer:_update_gun()
    GameplayStatic.debug_text("has gun")
    self.weapon_data:update()
end

function FpPlayer:_get_view_pos_and_dir()
    local look_dir = lMath.angles_to_vector(self.view_angles.x,self.view_angles.y)
    local pos = self.move:get_position()
    pos = vec_add(pos, {y=PLAYER_HEIGHT})
    return pos,look_dir
end
CONTROLLER_EXP = 2.0
function apply_controller_exp(val)
    exp = CONTROLLER_EXP
    if val < 0 then
        return -lMath.pow(-val,exp)
    else
        return lMath.pow(val,exp)
    end
end

MOUSE_SENSITIVITY = 0.005
MOVE_FRICTION = 8.0
MAX_SPEED = 5.0
ACCEL_VAL = 13.0
function FpPlayer:update()
    GameplayStatic.reset_debug_text_height()

    local dt = GameplayStatic.get_dt()

    -- update looking
    local lookX = lInput.get_con_axis(SDL_CONTROLLER_AXIS_RIGHTX)
    local lookY = lInput.get_con_axis(SDL_CONTROLLER_AXIS_RIGHTY)
    lookX = apply_controller_exp(controller_dead_zone(lookX,DEADZONE_VAL)) * SENSITIVITY
    lookY = apply_controller_exp(controller_dead_zone(lookY,DEADZONE_VAL)) * SENSITIVITY
    local moveX = controller_dead_zone(lInput.get_con_axis(SDL_CONTROLLER_AXIS_LEFTX),DEADZONE_VAL)
    local moveY = controller_dead_zone(lInput.get_con_axis(SDL_CONTROLLER_AXIS_LEFTY),DEADZONE_VAL)



    if self.game_focused then
        if lInput.was_key_pressed(SDL_SCANCODE_1) then
            self.weapon_data:switch_to(1)
        elseif lInput.was_key_pressed(SDL_SCANCODE_2) then
            self.weapon_data:switch_to(2)
        elseif lInput.was_key_pressed(SDL_SCANCODE_3) then
            self.weapon_data:switch_to(3)
        elseif lInput.was_key_pressed(SDL_SCANCODE_4) then
            self.weapon_data:switch_to(4)
        elseif lInput.was_key_pressed(SDL_SCANCODE_5) then
            self.weapon_data:switch_to(5)
        end
        local delta = lInput.get_mouse_delta()
        lookX = lookX + delta.x * MOUSE_SENSITIVITY
        lookY = lookY + delta.y * MOUSE_SENSITIVITY

        if lInput.is_key_down(SDL_SCANCODE_W) then
            moveY= -1
        end
        if lInput.is_key_down(SDL_SCANCODE_S) then
            moveY = moveY + 1
        end
         if lInput.is_key_down(SDL_SCANCODE_A) then
            moveX = -1
        end
        if lInput.is_key_down(SDL_SCANCODE_D) then
            moveX = moveX + 1
        end

        if lInput.was_key_pressed(SDL_SCANCODE_ESCAPE) then
            lInput.set_capture_mouse(false)
            self.game_focused = false
        end
    else
        if lInput.was_mouse_pressed(0) and not lInput.is_imgui_blocking_inputs() then
            self.game_focused = true
            lInput.set_capture_mouse(true)
        end
    end



    if lInput.was_con_button_pressed(SDL_CONTROLLER_BUTTON_DPAD_LEFT) then
        self.weapon_data:next_weapon()
    end
    if lInput.was_con_button_pressed(SDL_CONTROLLER_BUTTON_DPAD_RIGHT) then
        self.weapon_data:prev_weapon()
    end



    self.view_angles.x = math.clamp(self.view_angles.x - lookY,-0.5*math.pi+0.1,0.5*math.pi-0.1)
    self.view_angles.y = lMath.fmod(self.view_angles.y+lookX,2.0*math.pi)

    local look_dir = lMath.angles_to_vector(self.view_angles.x,self.view_angles.y)

    local move_dir = CopyInst(look_dir)
    move_dir.y = 0
    move_dir = normalize(move_dir)  -- ground plane
    local side_dir = cross(move_dir,{y=1})

    -- apply velocity changes
    local curSpeed = lMath.length(self.velocity)
    if curSpeed > 0.0001 then
        local dropAmt = curSpeed * MOVE_FRICTION * dt
        local newSpd = curSpeed - dropAmt
        if newSpd<0 then
            newSpd = 0
        end
        local factor = newSpd/curSpeed
        self.velocity.x = self.velocity.x * factor
        self.velocity.z = self.velocity.z * factor
    end




    local moveVec = {x=moveX,y=moveY}
    local moveVecLen = lMath.length(moveVec)
    if moveVecLen > 1 then
        moveVec = vec_multf(moveVec,1.0/moveVecLen)
        moveVecLen = 1
    end
    ---@type lVec3
    local xzVelocity = {x=self.velocity.x,y=0,z=self.velocity.z}
    local wishSpeed = moveVecLen * MAX_SPEED
    local wishDir = vec_add(vec_multf(look_dir,-moveVec.y),vec_multf(side_dir,moveVec.x))
    wishDir.y = 0
    local wishDirLen = lMath.length(wishDir)
    if wishDirLen>0.001 then
        wishDir = vec_multf(wishDir,1.0/wishDirLen)
    end
    local addSpeed = math.max(wishSpeed - lMath.dot(xzVelocity,wishDir),0)
    xzVelocity = vec_add(xzVelocity,vec_multf(wishDir,math.min(ACCEL_VAL*wishSpeed*dt,addSpeed)))
    local velLen = lMath.length(xzVelocity)

    GameplayStatic.debug_text("wishSpd:"..wishSpeed)

    if velLen<0.1 then
        xzVelocity = {x=0,y=0,z=0}
    end
    self.velocity.x = xzVelocity.x
    self.velocity.z = xzVelocity.z
    if not self.move:is_touching_down() then
        self.velocity.y = self.velocity.y - 9*dt
    end

    --local disp = vec_multf(move_dir,-moveVec.y*dt*MOVE_SPEED)
   -- disp = vec_add(disp,vec_multf(side_dir,moveVec.x*dt*MOVE_SPEED))

    local disp = vec_multf(self.velocity,dt)

    self.move:move(disp,dt,0.001)
    self.velocity = self.move:get_result_velocity()
    GameplayStatic.debug_text("vel:"..self.velocity.x)
    self:_update_gun()


    local pos = self.move:get_position()
    pos = vec_add(pos, {y=PLAYER_HEIGHT})

    pos = self.cam_shake:evaluate(pos,look_dir,dt)

    self.cam:get_owner():transform_look_at(pos,vec_add(pos,look_dir))
    self.cam:set_fov(85)
   -- GameplayStatic.debug_text("x="..self.view_angles.x)
    --GameplayStatic.debug_text("y="..self.view_angles.y)
   -- GameplayStatic.debug_text("lookdir.x="..look_dir.x)
   -- GameplayStatic.debug_text("lookdir.y="..look_dir.y)
   -- GameplayStatic.debug_text("lookdir.z="..look_dir.z)

   -- GameplayStatic.debug_text("lookX="..lookX)
   -- GameplayStatic.debug_text("lookY="..lookY)


    self:_draw_player_ui()

    if lInput.was_key_pressed(SDL_SCANCODE_F) then    
        local hit_result = GameplayStatic.cast_ray(pos,vec_add(pos,vec_multf(look_dir,2.0)),GameplayStatic.get_collision_mask_for_physics_layer(PL_DYNAMICOBJECT),self.physics)
        if hit_result.hit and hit_result.what then
            local door_obj = hit_result.what:get_component(FpDoor)
            local c = hit_result.what:get_component(MeshComponent)
 
            if door_obj~=nil then
                print("INTERACTING")
                door_obj:interact()
            end
            
        end
    end

    self:get_owner():set_ws_position(pos)
end
function FpPlayer:start()
    local cap = self:get_owner():create_component(CapsuleComponent)
    cap:set_body_type(BODYTYPE_KINEMATIC)
    cap:set_data(1.9,0.25,0.8)
    cap:set_physics_layer(PL_CHARACTER)
    --cap:set_is_trigger(true)
    self.physics = cap

    self.move = self:get_owner():create_component(CharacterMovementComponent)
    self.move:set_physics_body(self.physics)
    self.view_angles = lMath.vec_new(0)

    local cam_ent = GameplayStatic.spawn_entity()
    self.cam = cam_ent:create_component(CameraComponent)
    self.cam:set_is_enabled(true)

    self.fp_gun = GameplayStatic.spawn_entity():create_component(MeshComponent)
    self.fp_gun:get_owner():parent_to(self.cam:get_owner())

    self.cam_shake = self:get_owner():create_component(CameraShake)

   -- self.fp_gun_trail = GameplayStatic.spawn_entity():create_component(TrailComponent)
    --self.fp_gun_trail:get_owner():parent_to(self.fp_gun:get_owner())
    --self.fp_gun_trail:set_history(20,0.025)
    --self.fp_gun_trail:set_width(0.9,false)
    --self.fp_gun_trail:set_material(MaterialInstance.load("trail_particle.mm"))
    --self.fp_gun_trail:get_owner():set_ls_position({z=-1.0})

    self:set_ticking(true)

    self.weapon_data = WeaponsContainer.new(self)

    self.velocity = lMath.vec_new(0)
    self.game_focused = false
end
function FpPlayer:stop()

end

---@class FpEnemy : Component
FpEnemy = {
}
function FpEnemy:start()
    self.on_death = Signal.new()   -- signaled when killed
end
function FpEnemy:update()
    
end
function FpEnemy:stop()
    
end
function FpEnemy:kill()
    self.on_death:invoke(nil)
    self:get_owner():destroy_deferred()
end

function create_enemy_position(pos)
    local ent = GameplayStatic.spawn_entity()
    ent:set_ws_position(pos)
    local capsule = ent:create_component(CapsuleComponent)
    capsule:set_data(1.8,0.3,0.9)
    capsule:set_physics_layer(PL_CHARACTER)
    capsule:set_body_type(BODYTYPE_KINEMATIC)
    --capsule:set_is_trigger(true)
    
    local mesh = ent:create_component(MeshComponent)
    mesh:set_model(Model.load("cylinder_nose.cmdl"))
    mesh:set_material_override(MaterialInstance.load("top_down/enemy_mat.mi.mi"))

    ent:create_component(FpEnemy)

    print("CREATING ENEMY")

    return ent
end