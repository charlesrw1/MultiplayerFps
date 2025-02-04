### Configuration

See "Framework/Config.h"
The engine uses Config_Var's to define any configuration to the engine. This is similar to quake/source engine cvars.
It also uses Engine_Cmd's for defining commands to the engine.
The two main files to be aware of are "vars.txt" and "init.txt". These are executed one after the other at engine inititalization. Write any vars or commands you want in these to startup the engine.
One crucial one for a game is "g_entry_level". It takes in a string asset name for the first map to load when starting a game. Set this to your main menu or startup map.

Another important command is "start_ed", this starts an editor of an asset type. The command is as follows: `"start_ed" <AssetType> <file>`
<AssetType> is the type of asset, some options are "Map", "AnimGraph", "DataClass", "Model", and anything else. These are defined in the AssetMetadata struct, you can find more in AssetRegistry.h
<file> is the filename with extension (not fullpath) of the asset. So "start_ed Map mymap.txt" starts the editor for mymap.txt. If you put an empty string for the name, it creates a new document.
Another way to open editors is by DOUBLE CLICKING on any asset in the AssetBrowser imgui window. If the asset type has an editor (IEditorTool) associated with it, it opens itself in it. use this for quickly editing stuff!

Another useful command is "map <mapname>". This opens <mapname> map for playing. When the game starts up, it essentially does "map <g_entry_level.string>".

A useful rendering command is "ot <scale:float> <alpha:float> <mip:float> <texture_name>"
It outputs a texture or internal render texture to the screen for debugging. Use "cot" to clear it
Some "internal render texture names" are "_gbuffer0,_gbuffer1,"_gbuffer2","_csm_shadow"
Ex: "ot 1 1 0 _gbuffer0" outputs the normal buffer at full scale (covers the whole screen)

### AssetBrowser:

The asset browser is the main tool when creating levels. As said above, double click on an asset to open its editor. You can also drag some of the assets from the browser onto the viewport to instantiate it in the level editor.
All assets can be dragged and dropped on to "AssetPtr's"!! These allow very easy ways to set assets. An AssetPtr is a property field in the property grid that looks like a solid colored rectangle.
Drag and drop compatible asset types to the AssetPtr rectangle to set it. It automatically serializes and loads!


### Class System
The engine uses a global class system/registry. See ClassBase.h, ReflectionProp.h, and PropertyEd.h. 

Classes can be registered by defining in the header: 

`CLASS_H(<myclassname>, <parentclassname>) `
`public:`
`...`
`};`

And in a .cpp file:

`CLASS_IMPL(<myclassname>);`

Every class should implment a property list getter:

`static const PropertyInfoList* get_props() {`
`	START_PROPS(<myclassname>)`
`		...`
`	END_PROPS(<myclassname>)`
`}`

For more about properties, see ReflectionProp.h

Inside "..." use "REG_" macros defined in "ReflectionMacros.h" Some common ones:

`REG_FLOAT(...)`
`REG_INT(...)`
`REG_BOOL(...)`
`REG_ASSETPTR(...)`

the enum PROP_SERIALIZE,PROP_DEFAULT, PROP_EDITABLE defines if properties should serialize,be editable, or both
