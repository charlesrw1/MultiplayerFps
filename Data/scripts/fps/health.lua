-- FpsHealth: HP container with damage/death signals.
-- Runtime component; not editor-placeable.

---@class FpsDamageInfo
---@field amount number
---@field source Entity|nil
---@field hit_pos lVec3|nil

---@class FpsHealth : Component
FpsHealth = {
    ---@type number
    max_health = 100,
    ---@type number
    current = 100,
}

function FpsHealth:start()
    self.current = self.max_health
    self.on_damaged = Signal.new()
    self.on_death = Signal.new()
    self.dead = false
end

---@param info FpsDamageInfo
function FpsHealth:take_damage(info)
    if self.dead then return end
    self.current = self.current - info.amount
    self.on_damaged:invoke(info)
    if self.current <= 0 then
        self.current = 0
        self.dead = true
        self.on_death:invoke(info)
    end
end

---@param amount number
function FpsHealth:heal(amount)
    if self.dead then return end
    self.current = math.min(self.max_health, self.current + amount)
end

---@return boolean
function FpsHealth:is_dead()
    return self.dead
end
