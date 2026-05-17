-- FpsInventory: ordered list of FpsWeapon instances + active slot.
-- View-model mesh is parented to the owner (the player's camera entity is
-- supplied via :init_view).

---@class FpsInventory : Component
FpsInventory = {}

function FpsInventory:start()
    ---@type FpsWeapon[]
    self.weapons = {}
    self.active_index = 0
    self.view_parent = nil  -- camera entity
    self.view_mesh = nil    -- MeshComponent
end

---@param view_parent Entity
function FpsInventory:init_view(view_parent)
    self.view_parent = view_parent
    local mesh_ent = GameplayStatic.spawn_entity()
    mesh_ent:parent_to(view_parent)
    self.view_mesh = mesh_ent:create_component(MeshComponent)
    self.view_mesh:set_is_visible(false)
end

---@param id string
---@return boolean
function FpsInventory:add_weapon_by_id(id)
    local w = FpsWeaponRegistry_make(id)
    if w == nil then return false end
    -- Don't stack duplicates
    for _, existing in ipairs(self.weapons) do
        if existing.id == id then return false end
    end
    self.weapons[#self.weapons+1] = w
    if self.active_index == 0 then
        self:switch_to(1)
    end
    return true
end

---@param index integer
function FpsInventory:switch_to(index)
    if index < 1 or index > #self.weapons then return end
    if index == self.active_index then return end
    self.active_index = index
    self:_refresh_view_model()
end

function FpsInventory:next_weapon()
    if #self.weapons == 0 then return end
    local n = self.active_index + 1
    if n > #self.weapons then n = 1 end
    self:switch_to(n)
end

function FpsInventory:prev_weapon()
    if #self.weapons == 0 then return end
    local n = self.active_index - 1
    if n < 1 then n = #self.weapons end
    self:switch_to(n)
end

---@return FpsWeapon|nil
function FpsInventory:get_active()
    if self.active_index < 1 then return nil end
    return self.weapons[self.active_index]
end

---@param view_pos lVec3
---@param view_dir lVec3
function FpsInventory:fire_active(view_pos, view_dir)
    local w = self:get_active()
    if w == nil then return end
    w:fire(view_pos, view_dir, self:get_owner())
end

function FpsInventory:_refresh_view_model()
    local w = self:get_active()
    if w == nil or self.view_mesh == nil then
        if self.view_mesh ~= nil then self.view_mesh:set_is_visible(false) end
        return
    end
    if w.model_name == nil or w.model_name == "" then
        self.view_mesh:set_is_visible(false)
        return
    end
    local m = Model.load(w.model_name)
    if m == nil then
        self.view_mesh:set_is_visible(false)
        return
    end
    self.view_mesh:set_model(m)
    self.view_mesh:set_is_visible(true)
    local owner = self.view_mesh:get_owner()
    owner:set_ls_scale({x=w.model_scale, y=w.model_scale, z=w.model_scale})
    owner:set_ls_position_rotation(w.model_offset, lMath.from_euler(w.model_rotation))
end
