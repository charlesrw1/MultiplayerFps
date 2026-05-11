

local function create_particles()
    local fp = ParticleDefinition.create("fire")
    fp:print("HELLO FIRE")
    fp:set_rate(0.1)


    fp = ParticleDefinition.create("smoke_1")
    fp = ParticleDefinition.create("sparks_1")

    
end
create_particles()