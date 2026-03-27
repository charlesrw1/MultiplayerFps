---@class HudDrawer : Component
HudDrawer = {
    health = 100,
    ammo = 30,
    room_index = 1,
    room_total = 4,
}

function HudDrawer:start()
    self:set_ticking(true)
end

function HudDrawer:update()
    local screen = Gui.get_screen_size()
    local sw = screen.w
    local sh = screen.h

    -- Health bar background (dark red)
    Gui.set_color(0.4, 0, 0, 1)
    Gui.rectangle(10, sh - 30, 200, 20)

    -- Health bar fill (bright red)
    local fill_w = math.floor(200 * (self.health / 100))
    Gui.set_color(0.9, 0.1, 0.1, 1)
    Gui.rectangle(10, sh - 30, fill_w, 20)

    -- Health text
    Gui.set_color(1, 1, 1, 1)
    Gui.print("HP: " .. self.health, 14, sh - 27)

    -- Ammo (bottom-right)
    local ammo_str = "AMMO: " .. self.ammo
    local ammo_sz = Gui.measure_text(ammo_str)
    Gui.set_color(1, 1, 1, 1)
    Gui.print(ammo_str, sw - ammo_sz.w - 10, sh - 27)

    -- Crosshair (center)
    local cx = math.floor(sw / 2)
    local cy = math.floor(sh / 2)
    Gui.set_color(0, 1, 0, 1)
    Gui.line(cx - 20, cy, cx - 5, cy, 1)
    Gui.line(cx + 5,  cy, cx + 20, cy, 1)
    Gui.line(cx, cy - 20, cx, cy - 5, 1)
    Gui.line(cx, cy + 5,  cx, cy + 20, 1)
    Gui.circle(cx, cy, 2, 8)

    -- Room number (top-center)
    if self.room_index <= self.room_total then
        local room_str = "Room " .. self.room_index .. " / " .. self.room_total
        local room_sz = Gui.measure_text(room_str)
        Gui.set_color(1, 1, 1, 1)
        Gui.print(room_str, math.floor((sw - room_sz.w) / 2), 10)
    else
        -- Win screen
        local win_str = "YOU WIN!"
        local win_sz = Gui.measure_text(win_str)
        Gui.set_color(1, 1, 0, 1)
        Gui.print(win_str, math.floor((sw - win_sz.w) / 2), math.floor(sh / 2) - 20)
    end
end
