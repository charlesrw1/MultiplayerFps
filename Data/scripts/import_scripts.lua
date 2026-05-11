

---@class ThePrefabFactory : IPrefabFactory
ThePrefabFactory = {
}
function ThePrefabFactory:create(ent, name)
    if self.creations[name]~=nil then
        self.creations[name](ent)
        return true
    end
    return false
end


---@param ent Entity
---@param door_model string
---@param locked boolean
---@param start_state boolean
function create_door_ent(ent, door_model,locked,start_state)
    local m = ent:create_component(MeshComponent)
    m:set_model(Model.load(door_model))
	local body = ent:create_component(MeshColliderComponent)
	body:set_physics_layer(PL_DYNAMICOBJECT)
    local door = ent:create_component(FpDoor)
    door:set_state(locked,start_state)
end

-- an empty component, can attach random stuff to it for quick scripts. I LOVE LUA!!!
---@class EmptyComponent : Component
EmptyComponent = {
}

---@param ent Entity
---@param callback function
---@return EmptyComponent
function create_ticker(ent,callback)
    local c = ent:create_component(EmptyComponent)
    c.update = callback
    c:set_ticking(true)
    return c
end

---@param ent Entity
---@param gun_type integer
---@param gun_model string
function create_gun_spawner(ent,gun_type,gun_model)
    local sph = ent:create_component(SphereComponent)
    sph:set_is_trigger(true)
    ---@param arg PhysicsBodyEventArg
    add_physics_callback(sph, function (arg)
        if arg.entered_trigger and arg.who then
            local player = arg.who:get_component(FpPlayer)
            if player ~= nil then
                player.weapon_data:switch_to(gun_type)
            end
        end
    end)
    
    local subent = ent:create_child_entity()
    local m = subent:create_component(MeshComponent)
    m:set_model(Model.load(gun_model))

    ---@param obj EmptyComponent
    local empty = create_ticker(subent,function (obj)
        local t = GameplayStatic.get_time()
        obj:get_owner():set_ls_euler_rotation({y=t})
        local h = math.sin(t*1.5+0.5)*0.3
        obj:get_owner():set_ls_position({y=h})
    end)
end

---@param model Model
---@param out agBuilder
function create_basic_tree(model,out)
	local clipRoot = out:alloc(agClipNode)
	clipRoot:set_clip(model,"run_forward_unequip")
	clipRoot:set_looping(true)
    out:set_root(clipRoot)
end

---@return Entity
function CreateRagdollSwat()
    local ent = GameplayStatic.spawn_entity()
    assert(ent~=nil)

        local m = ent:create_component(MeshComponent)
        local model = Model.load("characters/swat_model/swat_model.cmdl")
       -- assert(model~=nil)
        m:set_model(model)
        --m:set_material_override(MaterialInstance.load("trigger_zone.mm"))
        -- m:set_is_visible(false)
        create_and_set_animator_tree(m,function (ctx)
            create_basic_tree(model,ctx)
        end)
        local animator = m:get_animator()
        if animator then
          --  animator:set_update_owner_position_to_root(true)
        end

        local ragdoll = ent:create_component(RagdollComponent)

        local mass_total = 0

        local create_physics_body = function (bone,rotation, height,radius,ofs,is_root)
            local child = GameplayStatic.spawn_entity()
            
            -- GameplayStatic.debug_break()
            child:set_ls_euler_rotation(rotation)
            
            local c = child:create_component(CapsuleComponent)
            c:set_data(height,radius*0.9,ofs)
            c:set_is_enable(false)
            c:set_is_static(false)
            c:set_is_simulating(true)
            c:set_density(700)
            c:set_material(PHYS_MAT_0)
            local mymass = c:get_mass()
            print("mymass="..mymass)
            mass_total = mass_total + mymass

            if is_root then
                ragdoll:add_root_body(bone,c)          
             else
                ragdoll:add_body(bone,c)
            end
           return child
        end

        local b1 = create_physics_body("mixamorig:Hips",{z=math.pi*0.5},0.3,0.15,0,true)
        --local b2 = create_physics_body("mixamorig:Spine",{z=math.pi*0.5},0.3,0.1,0)
        local b3 = create_physics_body("mixamorig:Spine1",{z=math.pi*0.5},0.3,0.15,0)
        local b4 = create_physics_body("mixamorig:Spine2",{z=math.pi*0.5},0.3,0.15,0)

        local rupleg = create_physics_body("mixamorig:RightUpLeg",{},0.45,0.1,0.225)
        local lupleg = create_physics_body("mixamorig:LeftUpLeg",{},0.45,0.1,0.225)
        local rleg = create_physics_body("mixamorig:RightLeg",{},0.45,0.1,0.3)
        local lleg = create_physics_body("mixamorig:LeftLeg",{},0.45,0.1,0.3)

        local ruparm = create_physics_body("mixamorig:RightArm",{},0.3,0.1,0.15)
        local rfarm = create_physics_body("mixamorig:RightForeArm",{},0.3,0.1,0.15)
        local luparm = create_physics_body("mixamorig:LeftArm",{},0.3,0.1,0.15)
        local lfarm = create_physics_body("mixamorig:LeftForeArm",{},0.3,0.1,0.15)
        local head = create_physics_body("mixamorig:Head",{},0.2,0.1,0.08)

        print("TOTAL MASS="..mass_total)

        ---@param bonea Entity
        ---@param boneb Entity
        local create_joint = function (bonea,boneb)

            local j = bonea:create_component(AdvancedJointComponent)
           -- j:set_rotation_joint_motion(JM_FREE,JM_LOCKED,JM_FREE)
          --  j:set_translate_joint_motion(JM_LOCKED,JM_LOCKED,JM_LOCKED)
            j:set_target(boneb)
            j:refresh_joint()
            return j
        end


      --  create_joint(b1,b2)
     --   create_joint(b2,b3)
     --   create_joint(b3,b4)

      --  create_joint(b1,rupleg)
        local enable_lupleg = true
        local enable_knee = true

        local JOINT_DAMP = 100
        local JOINT_STIFF = JOINT_DAMP*5

        local makeleg = function (upleg,downleg)
            local upleg_j = create_joint(upleg,b1)
            if enable_lupleg then
                upleg_j:set_rotation_joint_motion(JM_LIMITED,JM_LIMITED,JM_LIMITED)
                upleg_j:set_twist_vars(-1.7,1,JOINT_DAMP,JOINT_STIFF)
                upleg_j:set_cone_vars(0.6,0.7,JOINT_DAMP,JOINT_STIFF)
            end
            local kneej = create_joint(downleg,upleg)
    
            if enable_knee then
                kneej:set_rotation_joint_motion(JM_LIMITED,JM_LOCKED,JM_LOCKED)
                kneej:set_twist_vars(0,1.8,JOINT_DAMP,JOINT_STIFF)
            end
        end
        makeleg(lupleg,lleg)
        makeleg(rupleg,rleg)

        local SPINE_DAMP = JOINT_DAMP
        local SPINE_STIFF = JOINT_STIFF

        local enable_spine = true
        local makespinej = function (bot,up)
            local spinejoint = create_joint(bot,up)
            if enable_spine then
                spinejoint:set_rotation_joint_motion(JM_LOCKED,JM_LOCKED,JM_LIMITED)
                spinejoint:set_twist_vars(-0.2,0.2,SPINE_DAMP,SPINE_STIFF)
                spinejoint:set_cone_vars(0.8,0.1,SPINE_DAMP,SPINE_STIFF)
            end
        end
        makespinej(b1,b3)
        makespinej(b3,b4)

        local enable_arm = true

        local make_arms = function (uparm,farm)
            local shoulder = create_joint(uparm,b4)
            if enable_arm then
                
                shoulder:set_rotation_joint_motion(JM_LIMITED,JM_LOCKED,JM_LIMITED)
                shoulder:set_twist_vars(-1.2,1.4,JOINT_DAMP,JOINT_STIFF)
                shoulder:set_cone_vars(0,1.2,JOINT_DAMP,JOINT_STIFF)
            end
            local elbow = create_joint(farm,uparm)
            if enable_arm then
                elbow:set_joint_anchor({},lMath.from_euler({y=math.pi*0.5}),0)
                elbow:set_rotation_joint_motion(JM_LIMITED,JM_LOCKED,JM_LOCKED)
                elbow:set_twist_vars(-2.1,0.2,JOINT_DAMP,JOINT_STIFF)
                elbow:set_cone_vars(0,0.9,JOINT_DAMP,JOINT_STIFF)
            end
            
        end
        make_arms(luparm,lfarm)
        make_arms(ruparm,rfarm)

        local enable_head = true

        local headj = create_joint(head,b4)
        if enable_head then
            headj:set_rotation_joint_motion(JM_LIMITED,JM_LIMITED,JM_LIMITED)
            headj:set_twist_vars(-0.8,0.8,JOINT_DAMP,JOINT_STIFF)
            headj:set_cone_vars(0.2,0.7,JOINT_DAMP,JOINT_STIFF)
        end

    return ent
end

---@class LuaMiscFuncsImpl : LuaMiscFuncs
LuaMiscFuncsImpl = {
}
function LuaMiscFuncsImpl:create_ragdoll()
    return CreateRagdollSwat()
end


function ThePrefabFactory:start()
    self.creations = {}

    self:add("wood_door",function (e)
        create_door_ent(e,"door.cmdl",false,false)
    end)
    self:add("locked_wood_door",function (e)
        create_door_ent(e,"door.cmdl",true,false)
    end)

    self:add("shotgun_spawner",function (ent)
        create_gun_spawner(ent,WEAPON_SHOTGUN,"supershotgun.cmdl")
    end)
    self:add("gl_spawner",function (ent)
        create_gun_spawner(ent,WEAPON_GRENADE_LAUNCHER,"grenade_launcher.cmdl")
    end)

    ---@param ent Entity
    self:add("security_camera", function (ent)
        local subobj = ent:create_child_entity()
        subobj:set_ls_position({z=0.05})
        local mesh = subobj:create_component(MeshComponent)
        mesh:set_model(Model.load("camera_model.cmdl"))
        local spot = subobj:create_component(SpotLightComponent)
        spot:set_shadows(true)
        spot:set_color({r=255,g=20,b=20},3)

    end)
    
    ---@param ent Entity
    self:add("enemy_test",function (ent)
        local m = ent:create_component(MeshComponent)
        local model = Model.load("characters/swat_model/swat_model.cmdl")
        m:set_model(model)
        create_and_set_animator_tree(m,function (ctx)
            create_basic_tree(model,ctx)
        end)

        local cap = ent:create_component(CapsuleComponent)
        cap:set_is_static(false)
        cap:set_is_simulating(true)
        cap:set_data(1.8,0.25,0.9)

    end)

    ---@param ent Entity
    self:add("thing_1",function (ent)
        local c = ent:create_child_entity()
        local m = c:create_component(MeshComponent)
        m:set_model(Model.load("supershotgun.cmdl"))
    end)

    ---@param ent Entity
    self:add("ragdoll",function (ent)

        local m = ent:create_component(MeshComponent)
        local model = Model.load("characters/swat_model/swat_model.cmdl")
       -- assert(model~=nil)
        m:set_model(model)
        --m:set_material_override(MaterialInstance.load("trigger_zone.mm"))
        -- m:set_is_visible(false)
        create_and_set_animator_tree(m,function (ctx)
            create_basic_tree(model,ctx)
        end)
        local animator = m:get_animator()
        if animator then
          --  animator:set_update_owner_position_to_root(true)
        end
        if GameplayStatic.is_editor() then
            return
        end

        local ragdoll = ent:create_component(RagdollComponent)

        local mass_total = 0

        local create_physics_body = function (bone,rotation, height,radius,ofs,is_root)
            local child = GameplayStatic.spawn_entity()
            
            -- GameplayStatic.debug_break()
            child:set_ls_euler_rotation(rotation)
            
            local c = child:create_component(CapsuleComponent)
            c:set_data(height,radius*0.9,ofs)
            c:set_is_enable(false)
            c:set_is_static(false)
            c:set_is_simulating(true)
            c:set_density(700)
            c:set_material(PHYS_MAT_0)
            local mymass = c:get_mass()
            print("mymass="..mymass)
            mass_total = mass_total + mymass

            if is_root then
                ragdoll:add_root_body(bone,c)          
             else
                ragdoll:add_body(bone,c)
            end
           return child
        end

        local b1 = create_physics_body("mixamorig:Hips",{z=math.pi*0.5},0.3,0.15,0,true)
        --local b2 = create_physics_body("mixamorig:Spine",{z=math.pi*0.5},0.3,0.1,0)
        local b3 = create_physics_body("mixamorig:Spine1",{z=math.pi*0.5},0.3,0.15,0)
        local b4 = create_physics_body("mixamorig:Spine2",{z=math.pi*0.5},0.3,0.15,0)

        local rupleg = create_physics_body("mixamorig:RightUpLeg",{},0.45,0.1,0.225)
        local lupleg = create_physics_body("mixamorig:LeftUpLeg",{},0.45,0.1,0.225)
        local rleg = create_physics_body("mixamorig:RightLeg",{},0.45,0.1,0.3)
        local lleg = create_physics_body("mixamorig:LeftLeg",{},0.45,0.1,0.3)

        local ruparm = create_physics_body("mixamorig:RightArm",{},0.3,0.1,0.15)
        local rfarm = create_physics_body("mixamorig:RightForeArm",{},0.3,0.1,0.15)
        local luparm = create_physics_body("mixamorig:LeftArm",{},0.3,0.1,0.15)
        local lfarm = create_physics_body("mixamorig:LeftForeArm",{},0.3,0.1,0.15)
        local head = create_physics_body("mixamorig:Head",{},0.2,0.1,0.08)

        print("TOTAL MASS="..mass_total)

        ---@param bonea Entity
        ---@param boneb Entity
        local create_joint = function (bonea,boneb)

            local j = bonea:create_component(AdvancedJointComponent)
           -- j:set_rotation_joint_motion(JM_FREE,JM_LOCKED,JM_FREE)
          --  j:set_translate_joint_motion(JM_LOCKED,JM_LOCKED,JM_LOCKED)
            j:set_target(boneb)
            j:refresh_joint()
            return j
        end


      --  create_joint(b1,b2)
     --   create_joint(b2,b3)
     --   create_joint(b3,b4)

      --  create_joint(b1,rupleg)
        local enable_lupleg = true
        local enable_knee = true

        local JOINT_DAMP = 100
        local JOINT_STIFF = JOINT_DAMP*5

        local makeleg = function (upleg,downleg)
            local upleg_j = create_joint(upleg,b1)
            if enable_lupleg then
                upleg_j:set_rotation_joint_motion(JM_LIMITED,JM_LIMITED,JM_LIMITED)
                upleg_j:set_twist_vars(-1.7,1,JOINT_DAMP,JOINT_STIFF)
                upleg_j:set_cone_vars(0.6,0.7,JOINT_DAMP,JOINT_STIFF)
            end
            local kneej = create_joint(downleg,upleg)
    
            if enable_knee then
                kneej:set_rotation_joint_motion(JM_LIMITED,JM_LOCKED,JM_LOCKED)
                kneej:set_twist_vars(0,1.8,JOINT_DAMP,JOINT_STIFF)
            end
        end
        makeleg(lupleg,lleg)
        makeleg(rupleg,rleg)

        local SPINE_DAMP = JOINT_DAMP
        local SPINE_STIFF = JOINT_STIFF

        local enable_spine = true
        local makespinej = function (bot,up)
            local spinejoint = create_joint(bot,up)
            if enable_spine then
                spinejoint:set_rotation_joint_motion(JM_LOCKED,JM_LOCKED,JM_LIMITED)
                spinejoint:set_twist_vars(-0.2,0.2,SPINE_DAMP,SPINE_STIFF)
                spinejoint:set_cone_vars(0.8,0.1,SPINE_DAMP,SPINE_STIFF)
            end
        end
        makespinej(b1,b3)
        makespinej(b3,b4)

        local enable_arm = true

        local make_arms = function (uparm,farm)
            local shoulder = create_joint(uparm,b4)
            if enable_arm then
                
                shoulder:set_rotation_joint_motion(JM_LIMITED,JM_LOCKED,JM_LIMITED)
                shoulder:set_twist_vars(-1.2,1.4,JOINT_DAMP,JOINT_STIFF)
                shoulder:set_cone_vars(0,1.2,JOINT_DAMP,JOINT_STIFF)
            end
            local elbow = create_joint(farm,uparm)
            if enable_arm then
                elbow:set_joint_anchor({},lMath.from_euler({y=math.pi*0.5}),0)
                elbow:set_rotation_joint_motion(JM_LIMITED,JM_LOCKED,JM_LOCKED)
                elbow:set_twist_vars(-2.1,0.2,JOINT_DAMP,JOINT_STIFF)
                elbow:set_cone_vars(0,0.9,JOINT_DAMP,JOINT_STIFF)
            end
            
        end
        make_arms(luparm,lfarm)
        make_arms(ruparm,rfarm)

        local enable_head = true

        local headj = create_joint(head,b4)
        if enable_head then
            headj:set_rotation_joint_motion(JM_LIMITED,JM_LIMITED,JM_LIMITED)
            headj:set_twist_vars(-0.8,0.8,JOINT_DAMP,JOINT_STIFF)
            headj:set_cone_vars(0.2,0.7,JOINT_DAMP,JOINT_STIFF)
        end


       -- create_joint(rupleg,rleg)
        --assert(gTimerMgr~=nil)
        if gTimerMgr~=nil then
            print("!!!!!!!!!!!!!!!!!!!!!!! ADDING TIMER!!!!!!!!!!!!!!!!!!!!!!!!!!")
            gTimerMgr:add(1.0,function ()
             print("!!!!!!!!!!!!!!!!!!!!!!! CALLING TIMER!!!!!!!!!!!!!!!!!!!!!!!!!!")
                ragdoll:enable()

              --  print("enabling ragdoll")          
             --   GameplayStatic.enable_ragdoll_shared(ent,true)
            end)
        end






        --local j = b2:create_component(AdvancedJointComponent)
        --j:set_rotation_joint_motion(JM_LIMITED,JM_LIMITED,JM_LIMITED)
        --j:set_target(b1)
        --j:set_twist_vars(-0.9,0.9,0,0)
        --j:set_cone_vars(-0.95,0.8,0,0)
        --j:refresh_joint()
    end)

    ---@param ent Entity
    self:add("jeep",function (ent)
        local jeep = ent:create_component(MeshComponent)
        jeep:set_model(Model.load("car/jeep_body.cmdl"))
        local phys = ent:create_component(MeshColliderComponent)
        phys:set_is_static(false)
        phys:set_is_simulating(true)
        phys:set_material(PHYS_MAT_0)
    
        local wheel_y = 0.058
        local wheel_front_z = 0
        local wheel_back_z = -2.56
        local wheel_x_ofs = 0.8
        local wheel_mesh = Model.load("car/jeep_wheel.cmdl")
        local add_wheel = function (x,z)
            local wheelEnt = ent:create_child_entity()
            local wheel = wheelEnt:create_component(MeshComponent)
            wheel:set_model(wheel_mesh)
            wheelEnt:set_ls_position({x=x,y=wheel_y,z=z})
        end
        add_wheel(wheel_x_ofs,wheel_front_z)
        add_wheel(-wheel_x_ofs,wheel_front_z)
        add_wheel(wheel_x_ofs,wheel_back_z)
        add_wheel(-wheel_x_ofs,wheel_back_z)
    end)


    ---@param ent Entity
    self:add("out_of_bounds_trigger",function (ent)
        ent.weapon_type = WEAPON_NONE
        local submod = ent:create_child_entity()
        local mod = submod:create_component(MeshComponent)
        mod:set_model(Model.load("eng/cube.cmdl"))
        mod:set_material_override(MaterialInstance.load("trigger_zone.mm"))
        submod:set_ls_position({x=-0.5,y=-0.5,z=0.5})

        local phys = ent:create_component(BoxComponent)
        phys:set_is_static(true)
        phys:set_is_trigger(true)
        ---@param arg  PhysicsBodyEventArg
        add_physics_callback(phys,function (arg)
            print("ENTERED OUT OF BOUNDS")
            if arg.entered_trigger and arg.who then
              --  local pfb = PrefabAsset.load("jeep.pfb")
              --  local ent = GameplayStatic.spawn_prefab(pfb)
              --  ent:set_ws_position({y=20})

                local p = arg.who:get_component(FpPlayer)
                if p then
                    p.weapon_data:switch_to(ent.weapon_type)
                    p.move:set_position({y=10})
                end
            end

        end)

    end)

    
    for key, value in pairs(self.creations) do
        self:define_prefab(key..".pfb")
    end
end

---@param name string
---@param creation function
function ThePrefabFactory:add(name,creation)
    self.creations[name]=creation
end

