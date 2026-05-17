-- FpsPickup runtime components. Wired onto the data-component's owner entity
-- by FpsGameManager at level start. On overlap with the player, apply effect
-- and destroy the owner.

---@class FpsPickupBase : Component
FpsPickupBase = {
    ---@type number
    overlap_radius = 0.6,
}

function FpsPickupBase:start()
    self:set_ticking(true)
end

---@return boolean true if a player overlapped and effect applied
function FpsPickupBase:_check_overlap()
    local pos = self:get_owner():get_ws_position()
    local mask = GameplayStatic.get_collision_mask_for_physics_layer(PL_CHARACTER)
    local hits = GameplayStatic.sphere_overlap(pos, self.overlap_radius, mask)
    for _, e in ipairs(hits) do
        local p = e:get_component(FpsPlayer)
        if p ~= nil then
            return self:_apply_to_player(p)
        end
    end
    return false
end

---@param player FpsPlayer
---@return boolean
function FpsPickupBase:_apply_to_player(player) return false end

function FpsPickupBase:update()
    if self:_check_overlap() then
        self:get_owner():destroy()
    end
end

-- =========================================================================

---@class FpsHealthPickupRuntime : FpsPickupBase
FpsHealthPickupRuntime = CopyInst(FpsPickupBase)
FpsHealthPickupRuntime.amount = 25.0

---@param player FpsPlayer
---@return boolean
function FpsHealthPickupRuntime:_apply_to_player(player)
    local hp = player:get_owner():get_component(FpsHealth)
    if hp == nil then return false end
    if hp.current >= hp.max_health then return false end  -- at full HP, don't consume
    hp:heal(self.amount)
    return true
end

-- =========================================================================

---@class FpsWeaponPickupRuntime : FpsPickupBase
FpsWeaponPickupRuntime = CopyInst(FpsPickupBase)
FpsWeaponPickupRuntime.weapon_id = ""

---@param player FpsPlayer
---@return boolean
function FpsWeaponPickupRuntime:_apply_to_player(player)
    if self.weapon_id == "" then return false end
    local inv = player:get_owner():get_component(FpsInventory)
    if inv == nil then return false end
    return inv:add_weapon_by_id(self.weapon_id)
end
