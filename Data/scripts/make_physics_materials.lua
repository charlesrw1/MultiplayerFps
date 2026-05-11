---@param sf number
---@param df number
---@param r number
---@return PhysicsMaterialWrapper
function make_phys_material(sf,df,r)
    local something = ClassBase.alloc(PhysicsMaterialWrapper)
    something:set_friction(sf,df)
    something:set_restitution(r)
    return something
end
