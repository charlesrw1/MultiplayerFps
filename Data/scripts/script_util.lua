
function CopyInst(orig, copies)
    copies = copies or {}
    if type(orig) ~= "table" then
        return orig
    elseif copies[orig] then
        return copies[orig]
    end

    local copy = {}
    copies[orig] = copy
    for k, v in pairs(orig) do
        copy[CopyInst(k, copies)] = CopyInst(v, copies)
    end
    return copy
end

function PrintTable(t)
    for k, v in pairs(t) do
        print(k, v)
    end
end
---@class TimerManager
TimerManager = {
	objects = {}
}
function TimerManager.new()
	return CopyInst(TimerManager)
end

accum = 0


---@class BruhComponent : Component
BruhComponent = {
}
function BruhComponent:update()
	local sometable = {}
	assert(false,"failed func from bruhcomponent")
end

function TimerManager:tick(dt)

	accum = accum + dt
	if accum > 1 then
		local ent = GameplayStatic.spawn_entity()
		local c = ent:create_component(BruhComponent)
		c:update()

		assert(false,"adf")
				print("123")

		somer.asdf = asdf.d
		
	end

	local remove_these = {}
	for k,v in pairs(self.objects) do
		v.time = v.time - dt
		if v.time <= 0 then
			v.callback()
			remove_these[#remove_these+1]=k
		end
	end
	for index, value in ipairs(remove_these) do
		self.objects[value]=nil
	end
end
---@param time number
---@param func function
function TimerManager:add(time,func)
	self.objects[func] = {callback=func,time=time}
	--print("TimeMgr table:")
	--PrintTable(self.objects)
end
---@param func function
function TimerManager:remove(func)
	self.objects[func] = nil
end

---@class Signal
Signal = {
	objects = {},
}
---@return Signal
function Signal.new()
	local t = CopyInst(Signal)
	return t
end
---@param func function
function Signal:add(func)
	self.objects[func]=func
end
---@param arg table|nil
function Signal:invoke(arg)
	for k,v in pairs(self.objects) do
		v(arg)
	end
end
---@param func function
function Signal:remove(func)
	self.objects[func]=nil
end
