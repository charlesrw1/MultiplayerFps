# Materials 

Materials are seperated into 2 parts: the master material and the material instance. The master material provides shader code and input parameters. A material instance instances a master material and overrides the input parameters. Master materials are created in a text editor, while material instances can be edited in either a text editor or in the engine's visual material editor. 

Master materials have (.mm) extension and instances have (.mi) extension

## Making Master Materials

Master materials have a few parts: OPT parameters, VAR parameters, DOMAIN type, and the shader code. 

### OPT

Option parameters. (first option is default). Declared like: `OPT <what> <value>`

- `AlphaTested` ("false"/"true"): enables alpha testing. uses the Opacity input.

- `BlendMode` ("Opaque"/"Blend"/"Add"): enables transparency

- `LightingMode` ("Lit"/"Unlit"): disables lighting

- `ShowBackfaces` ("false"/"true"): if true, disables backface culling

### VAR

Input parameters. Declared like: `VAR <type> <option> <default_value>`. 
Variables are accessed like it was a GLSL variable. Available types:

- `texture2D`: texture available for sampling. Example: `VAR texture2D MyTexture "my_texture.dds"`

- `float`: 32bit float. Example: `VAR float MyFloat 0.0`

- `vec4`: 32bit color (uint8 percesion). Example: `VAR vec4 MyColor 255 255 255 255`

### DOMAIN

What shader type to use. Declared like: `DOMAIN <what>`

- `Default`: standard deferred shader

- `Decal`: used for decals

- `PostProcess`: used for postprocess materials

### Shader Inputs

When writing a master material, you have various shader inputs to use.

- `(vec4) g.viewpos_time`: xyz stores camera viewpos, w stores time

- `(vec4) g.viewfront`: xyz stores camera front dir

- `(mat4) g.viewproj`: view-projection matrix

- `(vec3) FS_IN_FragPos`: input world space position of fragment

- `(vec3) FS_IN_Normal`: non-normalized input world normal

- `(vec2) FS_IN_Texcoord`: input texture coordinate

### Shader Outputs

Outputs are the way master materials send data to the rest of the material system.

- `(vec3) WORLD_POSITION_OFFSET`: (for vertex shader only) offset applied to input vertex position, in world space.

- `(vec3) BASE_COLOR`: output albedo color

- `(float) ROUGHNESS`: output pbr roughness

- `(float) METALLIC`: output pbr metallic

- `(vec3) EMISSIVE`: output emissive color that gets added to final lit color

- `(vec3) NORMALMAP`: tangent space normalmap output

- `(float) OPACITY`: used for `AlphaTested` and transparency opacity.

- `(float) AOMAP`: small detail AO map, multiplied with `BASE_COLOR`










