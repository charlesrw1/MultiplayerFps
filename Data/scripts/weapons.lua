

---@class C4Object : Component
C4Object = {
}
function C4Object:start()
    self:set_ticking(true)
end

---@param objs Entity[]
---@param me Entity
function find_first_not_this(objs,me)
    for index, value in ipairs(objs) do
        if value~=me then
            return value
        end
    end
    return nil
end

function print_vec(str,v)
    print(str.." "..v.x.." "..v.y.." "..v.z)
end

function C4Object:update()
    local pos = self:get_owner():get_ws_position()
    ---@type Entity[]
    local objs = GameplayStatic.sphere_overlap(pos,0.2,GameplayStatic.get_collision_mask_for_physics_layer(PL_PHYSICSOBJECT))
    if #objs > 1 then
        local first_found = find_first_not_this(objs,self:get_owner())
        if first_found ~=  nil then
            local physics_body = self:get_owner():get_component(PhysicsBody)
            self:get_owner():get_component(TrailComponent):destroy_deferred()
            self:get_owner():parent_to(first_found)
            local vel = physics_body:get_linear_velocity()
            vel = normalize(vel)
            physics_body:destroy_deferred()
            -- cast ray
            local cast_result = GameplayStatic.cast_ray(pos,vec_add(pos,vec_multf(vel,0.5)),GameplayStatic.get_collision_mask_for_physics_layer(PL_PHYSICSOBJECT),physics_body)
            if cast_result.hit then
                local n = cast_result.normal
                local orthoVecs = lMath.make_orthogonal_vectors(n)

                self:get_owner():transform_look_at(cast_result.pos,vec_add(cast_result.pos,orthoVecs.up))
            else
                self:get_owner():set_ws_position(pos)
            end


        end
        self:set_ticking(false)
    end
end


WEAPON_NONE = 1
WEAPON_SHOTGUN = 2
WEAPON_GRENADE_LAUNCHER = 3
WEAPON_PHYSICS = 4
WEAPON_C4 = 5
WEAPON_COUNT = 5

WeaponNone = {
    ---@type FpPlayer
    parent = nil
}
---@class WeaponShotgun
WeaponShotgun = {
    modelname = "top_down/rifle.cmdl",
    offset = {x=0.1,y=-0.12,z=-0.12},
    rotation = {x=-0.1,y=0.05+math.pi,z=0},
    scale = 0.5,
    ---@type FpPlayer
    parent = nil,

    shoot_cooldown = 0.2,
    last_shoot_time = 0.0,
}
---@class WeaponGrenadeLauncher
WeaponGrenadeLauncher = {
    modelname = "grenade_launcher.cmdl",
    offset = {x=0.07,y=-0.06,z=-0.2},
    rotation = {x=0.1,y=0.05,z=0},
    scale = 0.1,
    ---@type FpPlayer
    parent = nil,


    shoot_cooldown = 0.2,
    last_shoot_time = 0.0,
}
---@class WeaponPhysics
WeaponPhysics = {
    modelname = "supershotgun.cmdl",
    offset = {x=0.07,y=-0.06,z=-0.2},
    rotation = {x=0.1,y=0.05,z=0},
    scale = 0.07,
    ---@type FpPlayer
    parent = nil,
}

---@class WeaponC4
WeaponC4 = {
    modelname = "c4_detonator.cmdl",
    offset = {x=0.07,y=-0.06,z=-0.1},
    rotation = {x=0.1,y=0.05,z=0},
    scale = 0.1,
    ---@type FpPlayer
    parent = nil,

    ---@type Entity[]
    c4_objects = {},
}

---@param item WeaponPhysics|WeaponGrenadeLauncher|WeaponShotgun|WeaponC4
function weapon_switch_shared(item)
    local model = Model.load(item.modelname)
    item.parent.fp_gun:set_is_visible(true)
    item.parent.fp_gun:set_model(model)
    item.parent.fp_gun:get_owner():set_ls_scale(Vec3.splat(item.scale))
    item.parent.fp_gun:get_owner():set_ls_position_rotation(item.offset,Quat.from_euler(item.rotation))
    item.parent.fp_gun:set_material_override(nil)
end

---@param parent FpPlayer
function weapon_does_want_shoot(parent)
    local wantsShoot = lInput.get_con_axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 0.5 or
        parent.game_focused and lInput.is_mouse_down(0)
     return wantsShoot
end

---@param parent  FpPlayer
function weapon_shoot_hitscan(parent)
    local vpos,vdir = parent:_get_view_pos_and_dir()
    local visMask = GameplayStatic.get_collision_mask_for_physics_layer(PL_VISIBLITY)
    local hitresult = GameplayStatic.cast_ray(vpos,vec_add(vpos,vec_multf(vdir,10.0)),visMask, parent.physics)
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

function WeaponNone:on_switch()
    self.parent.fp_gun:set_is_visible(false)
end
function WeaponNone:update()
end
function WeaponNone:on_end()
end

function WeaponC4:on_switch()
    weapon_switch_shared(self)
end
C4_IMPULSE = 0.05
function WeaponC4:update()
    if  self.parent.game_focused and lInput.was_mouse_pressed(0) then
        -- detonate
        ---@type FpPlayer[]
        local thePlayers = GameplayStatic.find_components(FpPlayer)
        ---@type FpPlayer
        local player = nil
        if #thePlayers > 0 then
            player = thePlayers[1]
        end

        for index, value in ipairs(self.c4_objects) do
            explode_shared(value:get_ws_position(),player)
            value:destroy_deferred()
        end
        self.c4_objects = {}
    end
    if self.parent.game_focused and lInput.was_mouse_pressed(2) then
        -- throw
        local vpos,vdir = self.parent:_get_view_pos_and_dir()

        local c4_obj = GameplayStatic.spawn_entity()
        c4_obj:set_ws_position(vpos)
        local mesh = c4_obj:create_component(MeshComponent)
        mesh:set_model(Model.load("c4_object.cmdl"))
        local phys = c4_obj:create_component(SphereComponent)
        phys:set_physics_layer(PL_PHYSICSOBJECT)
        phys:set_body_type(BODYTYPE_DYNAMIC)
        phys:set_radius(0.08)
        phys:apply_impulse(vpos,vec_multf(vdir,C4_IMPULSE))
        local trail = c4_obj:create_component(TrailComponent)
        trail:set_material(MaterialInstance.load("trail_particle.mm"))
        trail:set_width(0.2,false)
        trail:set_history(6,0.05)
        c4_obj:create_component(C4Object)
        
        self.c4_objects[#self.c4_objects+1] = c4_obj

    end
end
function WeaponC4:on_end()
    
end

function WeaponShotgun:on_switch()
    weapon_switch_shared(self)
end
function WeaponShotgun:update()
    if not weapon_does_want_shoot(self.parent) then
        return
    end

    local time_now = GameplayStatic.get_time()
    if self.last_shoot_time + self.shoot_cooldown < time_now then
        weapon_shoot_hitscan(self.parent)
        self.last_shoot_time = time_now
    end
end
function WeaponShotgun:on_end()
end

function WeaponPhysics:on_switch()
    weapon_switch_shared(self)
    self.parent.fp_gun:set_material_override(MaterialInstance.load("top_down/enemy_mat.mi.mi"))

    local theGun = self.parent.fp_gun:get_owner()
    local beamStart = GameplayStatic.spawn_entity()
    beamStart:parent_to(theGun)
    beamStart:set_ls_position({z=-10.0,y=-2})
    self.theBeam = beamStart:create_component(BeamComponent)
    self.theBeam:set_vars(true,false,500,0.1,0.1,MaterialInstance.load("beam_particle.mm"),8)
    self.theBeam:set_visible(false)
    self.has_pos = false
    self.manipulating_object = nil
    self.manip_dist = 0
    ---@type lQuat
    self.gun_angle = self:_get_player_view_quat()

end
function WeaponPhysics:_get_player_view_quat()
    local angles = self.parent.view_angles

    return Quat.from_euler(Vec3.new(angles.x,-angles.y-math.pi*0.5,0))
end

function WeaponPhysics:update()
    GameplayStatic.debug_text("weapon physics")
    local dt = GameplayStatic.get_dt()
    local cur_player_quat = self:_get_player_view_quat()
    self.gun_angle = lMath.damp_quat(cur_player_quat,self.gun_angle,0.001,dt)
    local as_euler = cur_player_quat:delta_to(self.gun_angle):to_euler()
    local delta_q = self.gun_angle:inverse() * cur_player_quat
    --self.parent.fp_gun:get_owner():set_ws_rotation(delta_q)
    --self.parent.fp_gun:get_owner():set_ws_position({y=1.5})
    self.parent.fp_gun:get_owner():set_ls_rotation(delta_q)
    local rot = cur_player_quat * delta_q
   


    if weapon_does_want_shoot(self.parent) then
        if not self.has_pos then 
            local vpos,vdir = self.parent:_get_view_pos_and_dir()
            local visMask = GameplayStatic.get_collision_mask_for_physics_layer(PL_VISIBLITY)
            local hitresult = GameplayStatic.cast_ray(vpos,vpos + vdir * 10.0,visMask, self.parent.physics)
            if hitresult.hit and hitresult.what then
                local pb = hitresult.what:get_component(PhysicsBody)
                if pb and pb:get_body_type() ~= BODYTYPE_STATIC then
                    self.theBeam:set_visible(true)
                    self.theBeam:set_target_pos(hitresult.pos)
                    self.theBeam:reset()
                    self.manipulating_object = pb
                    pb:set_body_type(BODYTYPE_KINEMATIC)
                    local manipPos = self.manipulating_object:get_owner():get_ws_position()
                    local toManip = manipPos - vpos
                    local toManipLen = toManip:length()
                    self.manip_dist = toManipLen
                end
            end
            self.has_pos = true
        else
            local vpos,vdir = self.parent:_get_view_pos_and_dir()
            if not GameplayStatic.is_null(self.manipulating_object) then
                as_euler = self.gun_angle:to_euler()
                --local rot = self.parent.fp_gun:get_owner():get_ws_rotation()

                local actual_gun_dir = rot * Vec3.new(0,0,-1)
                GameplayStatic.debug_line_normal({},actual_gun_dir,1,0,{})

                local newPos = vpos + actual_gun_dir * self.manip_dist
                local manipObj = self.manipulating_object:get_owner()
                local actualPos = lMath.damp_vector(newPos,manipObj:get_ws_position(),0.01,GameplayStatic.get_dt())
                manipObj:set_ws_position(actualPos)
                self.theBeam:set_target_pos(manipObj:get_ws_position())

                if lInput.was_mouse_pressed(2) then
                    self.manipulating_object:set_body_type(BODYTYPE_DYNAMIC)
                    self.manipulating_object:set_linear_velocity(actual_gun_dir * 10.0)
                    self.theBeam:set_visible(false)
                end
            else
                self.manipulating_object = nil
            end

        end
    else
        if not GameplayStatic.is_null(self.manipulating_object) then
            self.manipulating_object:set_body_type(BODYTYPE_DYNAMIC)
        end
        self.manipulating_object = nil

        self.theBeam:set_visible(false)
        self.has_pos = false
    end

end
function WeaponPhysics:on_end()
    self.theBeam:get_owner():destroy_deferred()
end
function WeaponGrenadeLauncher:on_switch()
    weapon_switch_shared(self)
end
function WeaponGrenadeLauncher:update()
    if not weapon_does_want_shoot(self.parent) then
        return
    end
    local time_now = GameplayStatic.get_time()
    if self.last_shoot_time + self.shoot_cooldown < time_now then
        self:_create_grenade()
        self.last_shoot_time = time_now
    end
end
function WeaponGrenadeLauncher:on_end()
    
end
function WeaponGrenadeLauncher:_create_grenade()
    local vpos,vdir = self.parent:_get_view_pos_and_dir()
    local sphere = GameplayStatic.spawn_entity()
    assert(sphere~=nil)
    sphere:set_ws_position(vpos)
    local grenade_model = Model.load("grenade.cmdl")
    sphere:create_component(MeshComponent):set_model(grenade_model)
    local trail = sphere:create_component(TrailComponent)
    trail:set_material(MaterialInstance.load("trail_particle.mm"))
    trail:set_width(0.2,false)
    trail:set_history(10,0.05)

    local phys = sphere:create_component(SphereComponent)
    phys:set_physics_layer(PL_PHYSICSOBJECT)
    phys:set_radius(0.2)
    phys:set_body_type(BODYTYPE_DYNAMIC)
    phys:apply_impulse(vpos,vec_multf(vdir,GRENADE_IMPULSE))


    local light = sphere:create_component(PointLightComponent)
    light:set_color({b=20,g=100},5.0)

    local flicker = sphere:create_component(LightFlicker)
    flicker.intensity = 5
    flicker.color = {b=20,g=100}

    sphere:create_component(Grenade)
end

---@class WeaponsContainer
WeaponsContainer = {
    data = {
    },
    cur_weapon = WEAPON_NONE,
    ---@type FpPlayer
    parent = nil,
}
---@param item integer
function WeaponsContainer:switch_to(item)
    if item==self.cur_weapon then
        return
    end
    self.data[self.cur_weapon]:on_end()
    self.cur_weapon = item
    self.data[self.cur_weapon].parent = self.parent
    self.data[self.cur_weapon]:on_switch()
end
function WeaponsContainer:next_weapon()
    local next = self.cur_weapon + 1
    if next > WEAPON_COUNT then
        next = 1
    end
    self:switch_to(next)
end
function WeaponsContainer:prev_weapon()
     local next = self.cur_weapon - 1
    if next <= 0 then
        next = WEAPON_COUNT
    end
    self:switch_to(next)
end
function WeaponsContainer:update()
    self.data[self.cur_weapon]:update()
end

---@param parent  FpPlayer
---@return WeaponsContainer
function WeaponsContainer.new(parent)
    local data = {
        CopyInst(WeaponNone),
        CopyInst(WeaponShotgun),
        CopyInst(WeaponGrenadeLauncher),
        CopyInst(WeaponPhysics),
        CopyInst(WeaponC4)
    }
    local weap_container = CopyInst(WeaponsContainer)
    weap_container.data = data
    weap_container.parent = parent
    PrintTable(data)
    return weap_container
end


