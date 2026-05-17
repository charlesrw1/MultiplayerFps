-- FpsPlayer: first-person controller. Owns capsule + CharacterMovementComponent
-- + camera entity. Reads mouse/keyboard, drives FpsInventory and FpsInteractor.

FPS_PLAYER_HEIGHT     = 1.75
FPS_PLAYER_RADIUS     = 0.30
FPS_PLAYER_EYE_HEIGHT = 1.55
FPS_PLAYER_MOVE_SPEED = 5.5
FPS_PLAYER_RUN_MULT   = 1.5
FPS_PLAYER_FRICTION   = 8.0
FPS_PLAYER_ACCEL      = 14.0
FPS_MOUSE_SENS        = 0.0035
FPS_CON_LOOK_SENS     = 2.5
FPS_DEADZONE          = 0.18
FPS_GRAVITY           = 9.81
FPS_JUMP_VEL          = 4.6
FPS_INTERACT_RANGE    = 2.5

---@param v number
---@return number
function fps_deadzone(v)
    if math.abs(v) < FPS_DEADZONE then return 0 end
    return v
end

---@class FpsPlayer : Component
FpsPlayer = {}

function FpsPlayer:start()
    self:set_ticking(true)

    local owner = self:get_owner()
    local cap = owner:create_component(CapsuleComponent)
    cap:set_data(FPS_PLAYER_HEIGHT, FPS_PLAYER_RADIUS, FPS_PLAYER_HEIGHT*0.5)
    cap:set_physics_layer(PL_CHARACTER)
    cap:set_is_static(false)
    self.capsule = cap

    self.move = owner:create_component(CharacterMovementComponent)
    self.move:set_physics_body(cap)
    self.move:set_capsule_info(FPS_PLAYER_HEIGHT, FPS_PLAYER_RADIUS, 0.05)

    local cam_ent = GameplayStatic.spawn_entity()
    self.cam_ent = cam_ent
    self.cam = cam_ent:create_component(CameraComponent)
    self.cam:set_is_enabled(true)

    self.view_pitch = 0.0
    self.view_yaw   = 0.0
    self.velocity   = {x=0, y=0, z=0}
    self.focused    = false
    self.keys       = {}    -- string set of held key ids (for locked doors)

    -- Game starts unfocused; first click captures mouse.
    lInput.set_capture_mouse(false)
end

---@param id string
function FpsPlayer:add_key(id)
    self.keys[id] = true
end

---@param id string
---@return boolean
function FpsPlayer:has_key(id)
    return self.keys[id] == true
end

---@return lVec3, lVec3
function FpsPlayer:get_view_pos_and_dir()
    local pos = self.move:get_position()
    pos = vec_add(pos, {x=0, y=FPS_PLAYER_EYE_HEIGHT, z=0})
    local dir = lMath.angles_to_vector(self.view_pitch, self.view_yaw)
    return pos, dir
end

function FpsPlayer:update()
    if gFpsManager ~= nil and gFpsManager.player == nil then
        gFpsManager.player = self
    end

    local dt = GameplayStatic.get_dt()

    -- Focus toggle
    if self.focused then
        if lInput.was_key_pressed(SDL_SCANCODE_ESCAPE) then
            self.focused = false
            lInput.set_capture_mouse(false)
        end
    else
        if lInput.was_mouse_pressed(0) and not lInput.is_imgui_blocking_inputs() then
            self.focused = true
            lInput.set_capture_mouse(true)
        end
    end

    -- ---- LOOK -----------------------------------------------------------
    local look_x = 0
    local look_y = 0
    local cx = fps_deadzone(lInput.get_con_axis(SDL_CONTROLLER_AXIS_RIGHTX))
    local cy = fps_deadzone(lInput.get_con_axis(SDL_CONTROLLER_AXIS_RIGHTY))
    look_x = look_x + cx * FPS_CON_LOOK_SENS * dt
    look_y = look_y + cy * FPS_CON_LOOK_SENS * dt
    if self.focused then
        local md = lInput.get_mouse_delta()
        look_x = look_x + md.x * FPS_MOUSE_SENS
        look_y = look_y + md.y * FPS_MOUSE_SENS
    end
    self.view_yaw   = lMath.fmod(self.view_yaw + look_x, math.pi*2.0)
    self.view_pitch = math.clamp(self.view_pitch - look_y, -0.5*math.pi+0.1, 0.5*math.pi-0.1)

    -- ---- MOVE INPUT -----------------------------------------------------
    local mx = fps_deadzone(lInput.get_con_axis(SDL_CONTROLLER_AXIS_LEFTX))
    local my = fps_deadzone(lInput.get_con_axis(SDL_CONTROLLER_AXIS_LEFTY))
    if self.focused then
        if lInput.is_key_down(SDL_SCANCODE_W) then my = -1 end
        if lInput.is_key_down(SDL_SCANCODE_S) then my =  1 end
        if lInput.is_key_down(SDL_SCANCODE_A) then mx = -1 end
        if lInput.is_key_down(SDL_SCANCODE_D) then mx =  1 end
    end

    local is_running = lInput.is_key_down(SDL_SCANCODE_LSHIFT)
    local max_speed = FPS_PLAYER_MOVE_SPEED * (is_running and FPS_PLAYER_RUN_MULT or 1.0)

    local look_dir = lMath.angles_to_vector(self.view_pitch, self.view_yaw)
    local move_dir = {x=look_dir.x, y=0, z=look_dir.z}
    move_dir = normalize(move_dir)
    local side_dir = cross(move_dir, {x=0, y=1, z=0})

    -- friction on horizontal velocity
    local hspd = math.sqrt(self.velocity.x*self.velocity.x + self.velocity.z*self.velocity.z)
    if hspd > 0.0001 then
        local drop = hspd * FPS_PLAYER_FRICTION * dt
        local new_hspd = math.max(0.0, hspd - drop)
        local f = new_hspd / hspd
        self.velocity.x = self.velocity.x * f
        self.velocity.z = self.velocity.z * f
    end

    local wish = vec_add(vec_multf(move_dir, -my), vec_multf(side_dir, mx))
    wish.y = 0
    local wlen = lMath.length(wish)
    if wlen > 0.001 then
        wish = vec_multf(wish, 1.0/wlen)
        local wish_speed = math.min(wlen, 1.0) * max_speed
        local xz = {x=self.velocity.x, y=0, z=self.velocity.z}
        local cur_in_wish = lMath.dot(xz, wish)
        local add = math.max(0.0, wish_speed - cur_in_wish)
        local accel = math.min(FPS_PLAYER_ACCEL * wish_speed * dt, add)
        self.velocity.x = self.velocity.x + wish.x * accel
        self.velocity.z = self.velocity.z + wish.z * accel
    end

    -- gravity / jump
    if self.move:is_touching_down() then
        if self.velocity.y < 0 then self.velocity.y = 0 end
        if self.focused and lInput.was_key_pressed(SDL_SCANCODE_SPACE) then
            self.velocity.y = FPS_JUMP_VEL
            if gFpsManager ~= nil then
                gFpsManager:report_noise(self.move:get_position(), 5.0)
            end
        end
    else
        self.velocity.y = self.velocity.y - FPS_GRAVITY * dt
    end

    self.move:move(vec_multf(self.velocity, dt), dt, 0.001)
    self.velocity = self.move:get_result_velocity()

    -- Running emits noise so guards can hear
    if is_running and hspd > 0.5 and gFpsManager ~= nil then
        gFpsManager:report_noise(self.move:get_position(), 8.0)
    end

    -- ---- CAMERA / VIEW --------------------------------------------------
    local eye_pos, eye_dir = self:get_view_pos_and_dir()
    self.cam_ent:transform_look_at(eye_pos, vec_add(eye_pos, eye_dir))
    self.cam:set_fov(85)
    self:get_owner():set_ws_position(self.move:get_position())

    -- ---- INVENTORY INPUT ------------------------------------------------
    local inv = self:get_owner():get_component(FpsInventory)
    if inv ~= nil and self.focused then
        if lInput.was_key_pressed(SDL_SCANCODE_1) then inv:switch_to(1) end
        if lInput.was_key_pressed(SDL_SCANCODE_2) then inv:switch_to(2) end
        if lInput.was_key_pressed(SDL_SCANCODE_3) then inv:switch_to(3) end
        if lInput.was_key_pressed(SDL_SCANCODE_4) then inv:switch_to(4) end
        if lInput.was_key_pressed(SDL_SCANCODE_Q) then inv:prev_weapon() end
        if lInput.was_key_pressed(SDL_SCANCODE_E) and lInput.is_key_down(SDL_SCANCODE_LCTRL) then
            inv:next_weapon()
        end
        local wants_fire = lInput.is_mouse_down(0) or lInput.get_con_axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 0.5
        if wants_fire then
            inv:fire_active(eye_pos, eye_dir)
        end
    end

    -- ---- INTERACT -------------------------------------------------------
    if self.focused and lInput.was_key_pressed(SDL_SCANCODE_F) then
        self:_try_interact(eye_pos, eye_dir)
    end

    self:_draw_hud()
end

---@param eye_pos lVec3
---@param eye_dir lVec3
function FpsPlayer:_try_interact(eye_pos, eye_dir)
    local mask = GameplayStatic.get_collision_mask_for_physics_layer(PL_DYNAMICOBJECT)
    local endpt = vec_add(eye_pos, vec_multf(eye_dir, FPS_INTERACT_RANGE))
    local res = GameplayStatic.cast_ray(eye_pos, endpt, mask, self.capsule)
    if not res.hit or res.what == nil then return end
    local door = res.what:get_component(FpsDoor)
    if door ~= nil then
        door:interact_with(self:get_owner())
        return
    end
end

function FpsPlayer:_draw_hud()
    local hp = self:get_owner():get_component(FpsHealth)
    if hp ~= nil then
        GameplayStatic.debug_text(string.format("HP: %d / %d", math.floor(hp.current), math.floor(hp.max_health)))
    end
    local inv = self:get_owner():get_component(FpsInventory)
    if inv ~= nil then
        local w = inv:get_active()
        if w ~= nil then
            GameplayStatic.debug_text("Weapon: "..w.display_name)
        else
            GameplayStatic.debug_text("Weapon: <none>")
        end
    end
end
